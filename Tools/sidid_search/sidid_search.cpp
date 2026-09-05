// sidid_search: identify C64 SID playroutines and vet sidid.cfg signatures
//
// Build: cmake --build --preset vs --config Release --target sidid_search
//
// Command-line compatible with player-id by Wilfred Bos
// (github.com/WilfredC64/player-id), output in the classic SIDId format:
//
//   sidid_search [directory, file or pattern to scan] [options]
//
//   -a             Scan all files, not just those with .sid extension
//   -c<threads>    Maximum CPU threads to be used (default is all)
//   -f<configfile> Configfile to use (env.variable SIDIDCFG can also be used)
//   -h             Scan HVSC location (env.variable HVSC), implies -s
//   -m             Scan each file for multiple signatures
//   -n             Show player info (use together with -p)
//   -o             List only unidentified files
//   -p<playername> Scan only for specific player
//   -s             Include subdirectories
//   -t             Truncate filenames
//   -u             List also unidentified files
//   -v             Verify signature config file
//   -wn            Write signatures in new format
//   -wo            Write signatures in old format
//   -x             Display hexadecimal offset of found signatures
//   -? or --help   Display usage information
//
// Both config formats parse: "&&" is accepted for "AND", END is optional.
//
// Extensions beyond the original:
//
//   --sig "20 ?? D4 AND A9 00 END"  Scan for an ad-hoc signature in sidid.cfg
//                  line syntax, to test a new entry before adding it to the
//                  config. Matches print the file offset, the C64 address
//                  (when the match is inside the load image) and which players
//                  the config also identifies the file as ("also:"), so
//                  collisions with existing signatures show up right away.
//
//   Scanning a single file prints every matching player with its match offset
//   and C64 address.

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "libSidplayEZ/src/EZ/sidid.h"

#include "ultra-shared/Config/PakFile.h"

#define SIDID_VERSION "1.1.0"

//-----------------------------------------------------------------------------

// The size comes from the directory entry (or a stat), so no seeking around
static bool loadFile ( const char* filename, const size_t size, std::string& out )
{
	const auto	file = std::fopen ( filename, "rb" );
	if ( ! file )
		return false;

	out.resize ( size );
	const auto	read = std::fread ( out.data (), 1, size, file );
	std::fclose ( file );

	return read == size;
}
//-----------------------------------------------------------------------------

static std::string loadFile ( const char* filename )
{
	std::error_code	ec;
	const auto	size = std::filesystem::file_size ( filename, ec );
	if ( ec || ! size )
		return {};

	std::string	str;
	if ( ! loadFile ( filename, size_t ( size ), str ) )
		return {};

	return str;
}
//-----------------------------------------------------------------------------

// Minimal PSID/RSID header peek so a match offset can be shown as a C64 address
struct SidLayout
{
	size_t		dataStart = 0;	// file offset where the C64 load image begins
	uint16_t	loadAddr = 0;
	bool		valid = false;
};

static SidLayout sidLayout ( const uint8_t* data, size_t length )
{
	SidLayout	layout;

	if ( length < 0x7E || ( std::memcmp ( data, "PSID", 4 ) && std::memcmp ( data, "RSID", 4 ) ) )
		return layout;

	const auto	dataOffset = size_t ( data[ 0x06 ] << 8 | data[ 0x07 ] );

	layout.loadAddr = uint16_t ( data[ 0x08 ] << 8 | data[ 0x09 ] );
	layout.dataStart = dataOffset;

	// A zero header address means the load address is the data's first word
	if ( ! layout.loadAddr && dataOffset + 2 <= length )
	{
		layout.loadAddr = uint16_t ( data[ dataOffset ] | data[ dataOffset + 1 ] << 8 );
		layout.dataStart += 2;
	}

	layout.valid = layout.dataStart <= length;

	return layout;
}
//-----------------------------------------------------------------------------

// Wildcard match for scan patterns: '*' and '?' don't cross '/', compares
// case-insensitively
static bool matchWild ( const std::string& pattern, const std::string& text )
{
	auto lower = [] ( char c ) { return char ( std::tolower ( uint8_t ( c ) ) ); };

	size_t	p = 0;
	size_t	t = 0;
	auto	starP = std::string::npos;
	size_t	starT = 0;

	while ( t < text.size () )
	{
		if ( p < pattern.size () && ( ( pattern[ p ] == '?' && text[ t ] != '/' ) || lower ( pattern[ p ] ) == lower ( text[ t ] ) ) )
		{
			p++;
			t++;
		}
		else if ( p < pattern.size () && pattern[ p ] == '*' )
		{
			starP = p++;
			starT = t;
		}
		else if ( starP != std::string::npos && text[ starT ] != '/' )
		{
			p = starP + 1;
			t = ++starT;
		}
		else
			return false;
	}

	while ( p < pattern.size () && pattern[ p ] == '*' )
		p++;

	return p == pattern.size ();
}
//-----------------------------------------------------------------------------

// -wn/-wo: rewrite the config in the new (&&, no END) or old (AND, END)
// format, in place like player-id does it
static int convertConfig ( const std::string& cfgPath, const bool newFormat )
{
	const auto	text = loadFile ( cfgPath.c_str () );
	if ( text.empty () )
	{
		std::println ( "Error: could not read {}", cfgPath );
		return EXIT_FAILURE;
	}

	const std::string	eol = text.find ( "\r\n" ) != std::string::npos ? "\r\n" : "\n";

	std::string	out;
	out.reserve ( text.size () );

	size_t	pos = 0;
	while ( pos < text.size () )
	{
		auto	end = text.find ( '\n', pos );
		if ( end == std::string::npos )
			end = text.size ();

		auto	line = text.substr ( pos, end - pos );
		pos = end + 1;

		while ( ! line.empty () && ( line.back () == '\r' || line.back () == ' ' ) )
			line.pop_back ();

		// Blank and name lines pass through
		if ( line.empty () || line.find ( ' ' ) == std::string::npos )
		{
			out += line;
			out += eol;
			continue;
		}

		// A signature line: swap the operator tokens and the END convention
		std::string	rebuilt;
		size_t	t = 0;
		while ( t < line.size () )
		{
			const auto	sp = line.find ( ' ', t );
			const auto	tokEnd = sp == std::string::npos ? line.size () : sp;
			if ( tokEnd > t )
			{
				const auto	tok = line.substr ( t, tokEnd - t );

				if ( tok == "END" )
					;	// dropped, re-added below for the old format
				else
				{
					if ( ! rebuilt.empty () )
						rebuilt += ' ';

					if ( tok == "AND" || tok == "&&" )
						rebuilt += newFormat ? "&&" : "AND";
					else
						rebuilt += tok;
				}
			}
			t = tokEnd + 1;
		}

		if ( ! newFormat )
			rebuilt += " END";

		out += rebuilt;
		out += eol;
	}

	auto	file = std::ofstream ( cfgPath, std::ios::out | std::ios::binary | std::ios::trunc );
	if ( ! file.is_open () )
	{
		std::println ( "Error: could not write {}", cfgPath );
		return EXIT_FAILURE;
	}
	file.write ( out.data (), std::streamsize ( out.size () ) );
	file.close ();

	std::println ( "{} written in {} format", cfgPath, newFormat ? "new" : "old" );
	return EXIT_SUCCESS;
}
//-----------------------------------------------------------------------------

// -v: lint the raw config text, in the spirit of player-id's validation.
// The parser itself is forgiving; this reports everything it forgives
static size_t verifyConfig ( const std::string& text )
{
	size_t	issues = 0;
	auto report = [ &issues ] ( size_t lineNo, const std::string& msg )
	{
		std::println ( "line {}: {}", lineNo, msg );
		issues++;
	};

	auto isHexByte = [] ( const std::string& t )
	{
		auto hex = [] ( char c ) { return ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ); };
		return t.size () == 2 && hex ( t[ 0 ] ) && hex ( t[ 1 ] );
	};
	auto isHexByteAnyCase = [] ( const std::string& t )
	{
		auto hex = [] ( char c ) { return ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'F' ) || ( c >= 'a' && c <= 'f' ); };
		return t.size () == 2 && hex ( t[ 0 ] ) && hex ( t[ 1 ] );
	};

	std::string					curName;
	auto						curHasSig = false;
	auto						emptyRun = 0;
	std::vector<std::string>	seenNames;	// lowercased, for duplicate detection

	auto toLower = [] ( std::string s )
	{
		for ( auto& c : s )
			c = char ( std::tolower ( uint8_t ( c ) ) );
		return s;
	};

	size_t	lineNo = 0;
	size_t	pos = 0;
	while ( pos < text.size () )
	{
		auto	end = text.find ( '\n', pos );
		if ( end == std::string::npos )
			end = text.size ();

		auto	raw = text.substr ( pos, end - pos );
		pos = end + 1;
		lineNo++;

		if ( ! raw.empty () && raw.back () == '\r' )
			raw.pop_back ();

		if ( raw.empty () )
		{
			if ( ++emptyRun == 2 )
				report ( lineNo, "two consecutive empty lines" );
			continue;
		}
		emptyRun = 0;

		if ( raw.find_first_not_of ( ' ' ) == std::string::npos )
		{
			report ( lineNo, "line contains only spaces" );
			continue;
		}
		if ( raw.front () == ' ' || raw.back () == ' ' )
			report ( lineNo, "leading or trailing spaces" );
		if ( raw.find ( "  " ) != std::string::npos )
			report ( lineNo, "double spaces" );
		if ( raw.find ( '\t' ) != std::string::npos )
			report ( lineNo, "tab character" );

		// Split into tokens
		std::vector<std::string>	tokens;
		size_t	t = 0;
		while ( t < raw.size () )
		{
			const auto	sp = raw.find ( ' ', t );
			const auto	tokEnd = sp == std::string::npos ? raw.size () : sp;
			if ( tokEnd > t )
				tokens.emplace_back ( raw.substr ( t, tokEnd - t ) );
			t = tokEnd + 1;
		}

		if ( tokens.size () == 1 )
		{
			// A player name
			const auto&	name = tokens[ 0 ];

			if ( name == "AND" || name == "&&" || name == "END" || name == "??" )
				report ( lineNo, "player name \"" + name + "\" is a reserved word" );
			if ( name.size () < 3 )
				report ( lineNo, "player name \"" + name + "\" is shorter than 3 characters" );

			if ( ! curName.empty () && ! curHasSig )
				report ( lineNo, "player \"" + curName + "\" has no signatures" );

			if ( std::ranges::find ( seenNames, toLower ( name ) ) != seenNames.end () )
				report ( lineNo, "player \"" + name + "\" defined more than once (or with different casing)" );
			seenNames.emplace_back ( toLower ( name ) );

			curName = name;
			curHasSig = false;
			continue;
		}

		// A signature line
		if ( curName.empty () )
			report ( lineNo, "signature without a player name" );
		else
			curHasSig = true;

		size_t	byteCount = 0;
		for ( size_t j = 0; j < tokens.size (); j++ )
		{
			const auto&	tok = tokens[ j ];
			const auto	last = j == tokens.size () - 1;

			if ( tok == "AND" || tok == "&&" )
			{
				if ( j == 0 )
					report ( lineNo, "signature begins with " + tok );
			}
			else if ( tok == "END" )
			{
				if ( ! last )
					report ( lineNo, "END is not at the end of the line" );
			}
			else if ( tok == "??" )
				byteCount++;
			else if ( isHexByte ( tok ) )
				byteCount++;
			else if ( isHexByteAnyCase ( tok ) )
				report ( lineNo, "lowercase hex value \"" + tok + "\"" );
			else
				report ( lineNo, "unsupported value \"" + tok + "\"" );
		}

		const auto	lastTok = tokens.back () == "END" && tokens.size () > 1 ? tokens[ tokens.size () - 2 ] : tokens.back ();
		if ( tokens.front () == "??" )
			report ( lineNo, "signature begins with a wildcard" );
		if ( lastTok == "??" )
			report ( lineNo, "signature ends with a wildcard" );
		if ( lastTok == "AND" || lastTok == "&&" )
			report ( lineNo, "signature ends with " + lastTok );

		if ( byteCount < 2 )
			report ( lineNo, "signature has fewer than 2 bytes" );
		if ( byteCount > 254 )
			report ( lineNo, "signature is larger than 254 bytes" );
	}

	if ( ! curName.empty () && ! curHasSig )
		report ( lineNo, "player \"" + curName + "\" has no signatures" );

	return issues;
}
//-----------------------------------------------------------------------------

static void printUsage ()
{
	std::println ( "Usage: sidid_search [directory, file or pattern to scan] [options]" );
	std::println ( "" );
	std::println ( "Options:" );
	std::println ( "-a             Scan all files, not just those with .sid extension" );
	std::println ( "-c<threads>    Maximum CPU threads to be used (default is all)" );
	std::println ( "-f<configfile> Configfile to use (env.variable SIDIDCFG can also be used)" );
	std::println ( "-h             Scan HVSC location (env.variable HVSC), implies -s" );
	std::println ( "-m             Scan each file for multiple signatures" );
	std::println ( "-n             Show player info (use together with -p)" );
	std::println ( "-o             List only unidentified files" );
	std::println ( "-p<playername> Scan only for specific player" );
	std::println ( "-s             Include subdirectories" );
	std::println ( "-t             Truncate filenames" );
	std::println ( "-u             List also unidentified files" );
	std::println ( "-v             Verify signature config file" );
	std::println ( "-wn            Write signatures in new format" );
	std::println ( "-wo            Write signatures in old format" );
	std::println ( "-x             Display hexadecimal offset of found signatures" );
	std::println ( "-? or --help   Display usage information" );
	std::println ( "" );
	std::println ( "Extensions:" );
	std::println ( "--sig \"20 ?? D4 AND A9 00 END\"  Scan for an ad-hoc signature (sidid.cfg" );
	std::println ( "               line syntax), printing match offsets and colliding players" );
}
//-----------------------------------------------------------------------------

int main ( int argc, char** argv )
{
	std::println ( "sidid_search {} - Copyright (c) 2026 Michael Hartmann", SIDID_VERSION );
	std::println ( "" );

	if ( argc < 2 )
	{
		printUsage ();
		return EXIT_SUCCESS;
	}

	std::string	root;
	std::string	cfgPath;
	std::string	playerName;
	std::string	sigLine;

	auto	allFiles = false;
	auto	includeSubdirs = false;
	auto	multiple = false;
	auto	onlyUnidentified = false;
	auto	listUnidentified = false;
	auto	scanHVSC = false;
	auto	verify = false;
	auto	showOffsets = false;
	auto	showInfo = false;
	auto	truncateNames = false;
	auto	threads = 0u;

	std::string	convertFormat;

	for ( auto i = 1; i < argc; i++ )
	{
		const std::string	a = argv[ i ];

		if ( a == "-?" || a == "--help" )		{ printUsage (); return EXIT_SUCCESS; }
		else if ( a == "--sig" && i + 1 < argc )	sigLine = argv[ ++i ];
		else if ( a == "-a" )					allFiles = true;
		else if ( a == "-s" )					includeSubdirs = true;
		else if ( a == "-h" )					scanHVSC = true;
		else if ( a == "-m" )					multiple = true;
		else if ( a == "-n" )					showInfo = true;
		else if ( a == "-o" )					onlyUnidentified = true;
		else if ( a == "-t" )					truncateNames = true;
		else if ( a == "-u" )					listUnidentified = true;
		else if ( a == "-v" )					verify = true;
		else if ( a == "-wn" )					convertFormat = "n";
		else if ( a == "-wo" )					convertFormat = "o";
		else if ( a == "-x" )					showOffsets = true;
		else if ( a.starts_with ( "-c" ) )
			threads = unsigned ( std::strtoul ( a.c_str () + 2, nullptr, 10 ) );
		else if ( a.starts_with ( "-f" ) )
			cfgPath = a.size () > 2 ? a.substr ( 2 ) : ( i + 1 < argc ? argv[ ++i ] : "" );
		else if ( a.starts_with ( "-p" ) )
			playerName = a.size () > 2 ? a.substr ( 2 ) : ( i + 1 < argc ? argv[ ++i ] : "" );
		else if ( a[ 0 ] == '-' )				{ std::println ( "Unknown option {}", a ); printUsage (); return EXIT_FAILURE; }
		else									root = a;
	}

	if ( scanHVSC )
	{
		const auto	env = std::getenv ( "HVSC" );
		if ( ! env )
		{
			std::println ( "Error: -h given but the HVSC environment variable is not set" );
			return EXIT_FAILURE;
		}
		root = env;
		includeSubdirs = true;
	}

	if ( root.empty () )
		root = std::filesystem::current_path ().string ();

	// Config: -f beats SIDIDCFG beats sidid.cfg in the current directory
	if ( cfgPath.empty () )
		if ( const auto env = std::getenv ( "SIDIDCFG" ) )
			cfgPath = env;
	if ( cfgPath.empty () )
		cfgPath = "sidid.cfg";

	// Show the resolved location, so a stray config elsewhere can't fool anybody
	std::error_code	ec;
	if ( const auto full = std::filesystem::absolute ( cfgPath, ec ); ! ec )
		cfgPath = full.lexically_normal ().string ();

	// -wn/-wo: convert the config file format in place and stop
	if ( ! convertFormat.empty () )
		return convertConfig ( cfgPath, convertFormat == "n" );

	// -v: lint the config and stop
	if ( verify )
	{
		const auto	text = loadFile ( cfgPath.c_str () );
		if ( text.empty () )
		{
			std::println ( "Error: could not read {}", cfgPath );
			return EXIT_FAILURE;
		}

		const auto	issues = verifyConfig ( text );
		if ( issues )
			std::println ( "{}: {} issue{} found", cfgPath, issues, issues == 1 ? "" : "s" );
		else
			std::println ( "{}: no issues found", cfgPath );

		return issues ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	sidid::database	config;
	if ( ! config.loadSidIDConfig ( cfgPath.c_str () ) )
	{
		std::println ( "Error: no signatures defined ({})", cfgPath );
		return EXIT_FAILURE;
	}
	const auto&	players = config.getPlayers ();

	std::println ( "Config: {}", cfgPath );
	if ( showInfo )
		std::println ( "Info:   {}", std::filesystem::path ( cfgPath ).replace_extension ( ".nfo" ).string () );
	std::println ( "" );

	// -s: restrict to one player (case-sensitive, like the original)
	auto	wantedPlayer = size_t ( -1 );
	if ( ! playerName.empty () )
	{
		for ( size_t p = 0; p < players.size (); p++ )
			if ( players[ p ].name == playerName )
			{
				wantedPlayer = p;
				break;
			}

		if ( wantedPlayer == size_t ( -1 ) )
		{
			std::println ( "Error: player \"{}\" not found in {}", playerName, cfgPath );
			return EXIT_FAILURE;
		}
	}

	// -n: show the sidid.nfo entry for the selected player
	if ( showInfo )
	{
		if ( wantedPlayer == size_t ( -1 ) )
			std::println ( "-n needs a player, use it together with -p\n" );
		else
		{
			const auto	nfoPath = std::filesystem::path ( cfgPath ).replace_extension ( ".nfo" ).string ();

			if ( ! config.loadSidIDInfo ( nfoPath.c_str () ) )
				std::println ( "No player info available ({})\n", nfoPath );
			else if ( const auto info = config.findPlayerInfo ( players[ wantedPlayer ].name ) )
			{
				auto printField = [] ( const std::string_view key, const std::string& value )
				{
					if ( value.empty () )
						return;

					size_t	pos = 0;
					auto	first = true;
					while ( pos <= value.size () )
					{
						auto	end = value.find ( '\n', pos );
						if ( end == std::string::npos )
							end = value.size ();

						if ( first )
							std::println ( "{:>9}: {}", key, value.substr ( pos, end - pos ) );
						else
							std::println ( "           {}", value.substr ( pos, end - pos ) );

						first = false;
						pos = end + 1;
					}
				};

				std::println ( "{}", info->player );
				printField ( "NAME", info->name );
				printField ( "AUTHOR", info->author );
				printField ( "RELEASED", info->released );
				printField ( "COMMENT", info->comment );
				printField ( "REFERENCE", info->reference );
				std::println ( "" );
			}
			else
				std::println ( "No info entry for {} in {}\n", players[ wantedPlayer ].name, nfoPath );
		}
	}

	// --sig: an ad-hoc signature to hunt for
	sidid::signature	wantedSig;
	if ( ! sigLine.empty () )
	{
		wantedSig = sidid::parseSignature ( sigLine );
		if ( wantedSig.empty () )
		{
			std::println ( "Error: could not parse signature \"{}\"", sigLine );
			return EXIT_FAILURE;
		}
	}

	// A scan target with wildcards selects files by pattern instead
	std::string	globPattern;
	if ( root.find_first_of ( "*?" ) != std::string::npos )
	{
		std::ranges::replace ( root, '\\', '/' );

		const auto	firstWild = root.find_first_of ( "*?" );
		const auto	slash = root.rfind ( '/', firstWild );

		globPattern = slash == std::string::npos ? root : root.substr ( slash + 1 );
		root = slash == std::string::npos ? "." : root.substr ( 0, slash );

		// A pattern with directory parts needs the recursive walk to reach them
		if ( globPattern.find ( '/' ) != std::string::npos )
			includeSubdirs = true;
	}

	// A zip corpus scans through its central directory
	PakFile	zipPak;
	auto	zipMode = false;
	size_t	zipPrefixLen = 0;

	{
		constexpr std::string_view	zipExt = ".zip";
		const auto	isZipName = root.size () > 4 && std::equal ( root.end () - 4, root.end (), zipExt.begin (), zipExt.end (),
																 [] ( char x, char y ) { return std::tolower ( uint8_t ( x ) ) == y; } );

		if ( globPattern.empty () && isZipName && std::filesystem::is_regular_file ( root ) )
		{
			zipMode = zipPak.open ( juce::File::getCurrentWorkingDirectory ().getChildFile ( juce::String ( root ) ) );

			if ( ! zipMode )
			{
				std::println ( "Error: could not read zip archive {}", root );
				return EXIT_FAILURE;
			}

			// A hand-made zip may wrap the collection in its folder
			if ( ! zipPak.exists ( "DOCUMENTS/HVSC.txt" ) && zipPak.exists ( "C64Music/DOCUMENTS/HVSC.txt" ) )
				zipPrefixLen = 9;
		}
	}

	// Collect the files to scan (zip archive, directory, pattern or a single file)
	const auto	singleFile = ! zipMode && globPattern.empty () && std::filesystem::is_regular_file ( root );

	auto isWantedFile = [ allFiles, &globPattern, &root ] ( const std::filesystem::path& p )
	{
		// A bare filename pattern applies at any depth, one with directory
		// parts matches the path relative to the scan base
		if ( ! globPattern.empty () )
		{
			if ( globPattern.find ( '/' ) != std::string::npos )
				return matchWild ( globPattern, std::filesystem::relative ( p, root ).generic_string () );

			return matchWild ( globPattern, p.filename ().string () );
		}

		if ( allFiles )
			return true;

		const auto	ext = p.extension ().string ();
		constexpr std::string_view	sid = ".sid";
		return std::equal ( ext.begin (), ext.end (), sid.begin (), sid.end (),
							[] ( char x, char y ) { return std::tolower ( uint8_t ( x ) ) == y; } );
	};

	// Path strings, display offsets and file sizes are prepared here, so the
	// workers only load and match
	struct FileEntry
	{
		std::string	path;		// generic separators
		size_t		relOffset;	// the displayed name starts here
		size_t		size;
	};

	auto	rootPrefix = std::filesystem::path ( root ).generic_string ();
	if ( ! rootPrefix.ends_with ( '/' ) )
		rootPrefix += '/';

	std::vector<FileEntry>	entries;
	try
	{
		auto addEntry = [ &entries, &rootPrefix ] ( const std::filesystem::directory_entry& e )
		{
			std::error_code	ec;
			const auto	size = e.file_size ( ec );
			if ( ec )
				return;

			auto	path = e.path ().generic_string ();
			const auto	rel = path.starts_with ( rootPrefix ) ? rootPrefix.size () : 0;
			entries.push_back ( { std::move ( path ), rel, size_t ( size ) } );
		};

		if ( zipMode )
		{
			for ( const auto& e : zipPak.getEntries () )
			{
				auto		path = e.path.toStdString ();
				const auto	rel = std::string_view ( path ).substr ( std::min ( zipPrefixLen, path.size () ) );

				if ( ! allFiles )
				{
					constexpr std::string_view	sid = ".sid";
					if ( rel.size () < 4 || ! std::equal ( rel.end () - 4, rel.end (), sid.begin (), sid.end (),
														   [] ( char x, char y ) { return std::tolower ( uint8_t ( x ) ) == y; } ) )
						continue;
				}

				entries.push_back ( { std::move ( path ), zipPrefixLen, size_t ( e.uncompressedSize ) } );
			}
		}
		else if ( singleFile )
		{
			std::error_code	ec;
			const auto	size = std::filesystem::file_size ( root, ec );
			auto	path = std::filesystem::path ( root ).generic_string ();
			const auto	slash = path.rfind ( '/' );
			entries.push_back ( { std::move ( path ), slash == std::string::npos ? 0 : slash + 1, ec ? 0 : size_t ( size ) } );
		}
		else if ( includeSubdirs )
		{
			for ( auto it = std::filesystem::recursive_directory_iterator ( root ); it != std::filesystem::recursive_directory_iterator (); ++it )
				if ( it->is_regular_file () && isWantedFile ( it->path () ) )
					addEntry ( *it );
		}
		else
		{
			for ( const auto& e : std::filesystem::directory_iterator ( root ) )
				if ( e.is_regular_file () && isWantedFile ( e.path () ) )
					addEntry ( e );
		}
	}
	catch ( const std::exception& e )
	{
		std::println ( "Error: {}", e.what () );
		return EXIT_FAILURE;
	}

	std::ranges::sort ( entries, {}, &FileEntry::path );

	// Scan: the files are split across worker threads, and each file's output
	// goes into its own buffer so the listing keeps its order
	struct Stats
	{
		size_t				identified = 0;
		size_t				unidentified = 0;
		size_t				examined = 0;
		std::vector<size_t>	playerCounts;
	};

	struct Scratch
	{
		std::string								data;
		std::vector<sidid::database::candidate>	cands;
	};

	auto processFile = [ & ] ( const FileEntry& entry, std::string& out, Stats& st, Scratch& scratch )
	{
		if ( ! entry.size )
			return;

		if ( zipMode )
		{
			const auto	mb = zipPak.load ( juce::String ( entry.path ) );
			if ( mb.getSize () != entry.size )
				return;

			scratch.data.assign ( static_cast<const char*> ( mb.getData () ), mb.getSize () );
		}
		else if ( ! loadFile ( entry.path.c_str (), entry.size, scratch.data ) )
			return;

		st.examined++;

		const auto	data = reinterpret_cast<const uint8_t*> ( scratch.data.data () );
		const auto	length = scratch.data.size ();

		auto	rel = std::string_view ( entry.path ).substr ( entry.relOffset );

		// -t: cut to the column width, as player-id does
		if ( truncateNames && rel.size () > 56 )
			rel = rel.substr ( 0, 56 );

		auto	line = std::back_inserter ( out );

		// --sig extension: match offset, C64 address and colliding players
		if ( ! wantedSig.empty () )
		{
			const auto	offset = sidid::findSignature ( data, length, wantedSig );
			if ( ! offset )
				return;

			st.identified++;

			std::string	ids;
			auto		lastPlayer = size_t ( -1 );
			config.findCandidates ( data, length, scratch.cands );
			for ( const auto& cand : scratch.cands )
			{
				if ( cand.player == lastPlayer )
					continue;
				if ( ! sidid::findSignature ( data, length, *cand.sig ) )
					continue;

				lastPlayer = cand.player;
				st.playerCounts[ cand.player ]++;

				if ( ! ids.empty () )
					ids += ", ";
				ids += players[ cand.player ].name;
			}

			const auto	layout = sidLayout ( data, length );
			if ( layout.valid && *offset >= layout.dataStart )
				std::format_to ( line, "{:<56} @ {:#06x} (${:04x})", rel, *offset, unsigned ( layout.loadAddr + ( *offset - layout.dataStart ) ) & 0xFFFF );
			else
				std::format_to ( line, "{:<56} @ {:#06x} (header)", rel, *offset );

			std::format_to ( line, "{}{}\n", ids.empty () ? "" : "  also: ", ids );
			return;
		}

		// Single-file extension: every matching player with its match offset
		if ( singleFile )
		{
			const auto	layout = sidLayout ( data, length );

			auto	lastPlayer = size_t ( -1 );
			config.findCandidates ( data, length, scratch.cands );
			for ( const auto& cand : scratch.cands )
			{
				if ( cand.player == lastPlayer )
					continue;
				if ( wantedPlayer != size_t ( -1 ) && cand.player != wantedPlayer )
					continue;

				const auto	offset = sidid::findSignature ( data, length, *cand.sig );
				if ( ! offset )
					continue;

				lastPlayer = cand.player;
				st.playerCounts[ cand.player ]++;

				if ( layout.valid && *offset >= layout.dataStart )
					std::format_to ( line, "{:<56} @ {:#06x} (${:04x})\n", players[ cand.player ].name, *offset, unsigned ( layout.loadAddr + ( *offset - layout.dataStart ) ) & 0xFFFF );
				else
					std::format_to ( line, "{:<56} @ {:#06x} (header)\n", players[ cand.player ].name, *offset );
			}

			if ( lastPlayer != size_t ( -1 ) )
				st.identified++;
			else
			{
				st.unidentified++;
				std::format_to ( line, "{:<56} *Unidentified*\n", rel );
			}
			return;
		}

		// SIDId-compatible directory scan
		auto	found = false;

		const auto	layout = sidLayout ( data, length );

		// -x appends the match offset (and the C64 address when possible)
		auto printMatch = [ showOffsets, &layout, &line ] ( const std::string_view file, const std::string& name, size_t offset )
		{
			if ( ! showOffsets )
			{
				std::format_to ( line, "{:<56} {}\n", file, name );
				return;
			}

			std::format_to ( line, "{:<56} {:<24}", file, name );
			if ( layout.valid && offset >= layout.dataStart )
				std::format_to ( line, " @ {:#06x} (${:04x})\n", offset, unsigned ( layout.loadAddr + ( offset - layout.dataStart ) ) & 0xFFFF );
			else
				std::format_to ( line, " @ {:#06x} (header)\n", offset );
		};

		if ( wantedPlayer != size_t ( -1 ) )
		{
			// -p: only the wanted player's signatures
			for ( const auto& sig : players[ wantedPlayer ].sigs )
				if ( const auto offset = sidid::findSignature ( data, length, sig ) )
				{
					found = true;
					st.playerCounts[ wantedPlayer ]++;
					if ( ! onlyUnidentified )
						printMatch ( rel, players[ wantedPlayer ].name, *offset );
					break;
				}
		}
		else
		{
			auto	lastPlayer = size_t ( -1 );
			config.findCandidates ( data, length, scratch.cands );
			for ( const auto& cand : scratch.cands )
			{
				if ( cand.player == lastPlayer )
					continue;

				const auto	offset = sidid::findSignature ( data, length, *cand.sig );
				if ( ! offset )
					continue;

				lastPlayer = cand.player;
				st.playerCounts[ cand.player ]++;

				if ( ! onlyUnidentified )
					printMatch ( found ? "" : rel, players[ cand.player ].name, *offset );

				found = true;
				if ( ! multiple )
					break;
			}
		}

		if ( found )
			st.identified++;
		else
		{
			st.unidentified++;
			if ( onlyUnidentified || listUnidentified )
				std::format_to ( line, "{:<56} *Unidentified*\n", rel );
		}
	};

	auto	threadCount = threads ? threads : std::thread::hardware_concurrency ();
	threadCount = std::clamp<unsigned> ( threadCount, 1, unsigned ( std::max<size_t> ( entries.size (), 1 ) ) );

	std::vector<std::string>	output ( entries.size () );
	std::vector<Stats>			stats ( threadCount );
	for ( auto& st : stats )
		st.playerCounts.assign ( players.size (), 0 );

	{
		std::atomic<size_t>			next { 0 };
		std::vector<std::jthread>	pool;
		for ( unsigned t = 0; t < threadCount; t++ )
			pool.emplace_back ( [ &, t ]
			{
				Scratch	scratch;
				for ( auto i = next.fetch_add ( 1 ); i < entries.size (); i = next.fetch_add ( 1 ) )
					processFile ( entries[ i ], output[ i ], stats[ t ], scratch );
			} );
	}

	for ( const auto& out : output )
		if ( ! out.empty () )
			std::print ( "{}", out );

	size_t				identified = 0;
	size_t				unidentified = 0;
	size_t				examined = 0;
	std::vector<size_t>	playerCounts ( players.size (), 0 );

	for ( const auto& st : stats )
	{
		identified += st.identified;
		unidentified += st.unidentified;
		examined += st.examined;
		for ( size_t p = 0; p < playerCounts.size (); p++ )
			playerCounts[ p ] += st.playerCounts[ p ];
	}

	// Summary, in the original's format
	std::println ( "" );
	for ( size_t p = 0; p < players.size (); p++ )
		if ( playerCounts[ p ] )
			std::println ( "{:<24} {}", players[ p ].name, playerCounts[ p ] );

	std::println ( "" );
	std::println ( "Identified               {}", identified );
	std::println ( "Unidentified             {}", unidentified );
	std::println ( "Total files examined     {}", examined );

	return EXIT_SUCCESS;
}
