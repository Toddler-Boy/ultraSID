// STIL quote-detector scan: diagnostic and regression harness for
// Database/STIL_Quotes.cpp. Parses the real STIL.txt with the app's line
// merging, runs splitCommentQuotes over every COMMENT and reports statistics,
// inner-quote cases and a few watched entries in full.
// The merging MIRRORS HVSC_database::loadSTIL and mergeComment; keep in sync.
// Tests/data-roots.txt says where $HVSC$ points (see sidplay_ab_test.cpp).
// Exit 0: detector healthy. Exit 1: a quote kept its enclosing quote
// characters, or the detected count collapsed.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Database/STIL_Quotes.cpp"

namespace fs = std::filesystem;

//-----------------------------------------------------------------------------

static std::vector<std::string> readLines ( const fs::path& path )
{
	std::ifstream	f ( path, std::ios::binary );

	std::vector<std::string>	lines;
	std::string	line;

	while ( std::getline ( f, line ) )
	{
		if ( ! line.empty () && line.back () == '\r' )
			line.pop_back ();

		lines.push_back ( line );
	}
	return lines;
}
//-----------------------------------------------------------------------------

struct Comment
{
	std::string	file;
	int			tune;
	std::string	body;
};
//-----------------------------------------------------------------------------

// Mirror of HVSC_database's field parsing and COMMENT line merging
static void parseStil ( const std::vector<std::string>& lines, std::vector<Comment>& out )
{
	std::string	tuneFilename;
	int			tuneNo = 0;

	std::vector<std::pair<std::string, std::string>>	fields;
	std::vector<std::string>							pendingLines;

	auto rtrim = [] ( std::string s ) {	while ( ! s.empty () && ( s.back () == ' ' || s.back () == '\t' ) ) s.pop_back (); return s;	};
	auto ltrim = [] ( std::string s ) {	s.erase ( 0, s.find_first_not_of ( " \t" ) ); return s;	};

	auto startsLongQuote = [] ( const std::string& str )
	{
		if ( str.empty () || str[ 0 ] != '"' )
			return false;

		const auto	pos = str.find ( '"', 1 );
		return pos == std::string::npos || int ( pos ) >= 20;
	};

	auto mergeField = [ & ] ()
	{
		if ( fields.empty () || pendingLines.empty () )
		{
			pendingLines.clear ();
			return;
		}

		auto&	[ fname, value ] = fields.back ();

		auto	wrapWidth = 78;
		for ( const auto& l : pendingLines )
			wrapWidth = std::max ( wrapWidth, int ( rtrim ( l ).size () ) );

		auto	prevTotal = 9 + int ( rtrim ( value ).size () );
		auto	prevText = rtrim ( value );
		auto	blankBreak = false;

		for ( const auto& raw : pendingLines )
		{
			// a blank line in the source is an explicit paragraph break
			if ( rtrim ( raw ).empty () )
			{
				blankBreak = true;
				continue;
			}

			const auto	line = raw.substr ( 9 );
			const auto	trimmed = ltrim ( rtrim ( line ) );

			auto isBreak = [ & ] () -> bool
			{
				if ( startsLongQuote ( line ) )
					return true;

				if ( ! line.empty () && line[ 0 ] == ' ' )		// authored extra indent
					return true;

				const auto	prevLen = prevTotal - 9;
				if ( prevLen < 50 || ( ! prevText.empty () && prevText.back () == '.' && prevLen < 60 ) )
					return true;

				const auto	sp = trimmed.find ( ' ' );
				const auto	firstWord = sp == std::string::npos ? trimmed : trimmed.substr ( 0, sp );
				if ( prevTotal + 1 + int ( firstWord.size () ) > wrapWidth )
					return false;

				const auto	prevEnd = prevText.empty () ? char ( 0 ) : prevText.back ();
				const auto	nextStart = trimmed.empty () ? char ( 0 ) : trimmed[ 0 ];

				return ( std::string ( ".!?:;\")" ).find ( prevEnd ) != std::string::npos || prevText.ends_with ( ".sid" ) )
					&& ( ( nextStart >= 'A' && nextStart <= 'Z' ) || ( nextStart >= '0' && nextStart <= '9' )
						|| nextStart == '"' || nextStart == '(' );
			};

			if ( fname == "COMMENT" )
				value += ( blankBreak || isBreak () ? "\n\n" : " " ) + line;
			else
				value += " " + line;
			blankBreak = false;

			prevTotal = int ( rtrim ( raw ).size () );
			prevText = rtrim ( line );
		}
		pendingLines.clear ();
	};

	auto flushFields = [ & ] ()
	{
		mergeField ();

		for ( auto& [ name, value ] : fields )
			if ( name == "COMMENT" )
				out.push_back ( { tuneFilename, tuneNo, value } );

		fields.clear ();
	};

	for ( const auto& s : lines )
	{
		if ( ! s.empty () && s[ 0 ] == '#' )
			continue;
		if ( s.empty () )
		{
			// blank line inside a wrapped field: explicit paragraph break
			if ( ! fields.empty () )
				pendingLines.push_back ( s );
			continue;
		}

		if ( s[ 0 ] == '/' )
		{
			flushFields ();
			tuneFilename = s;
			tuneNo = 0;
			continue;
		}

		if ( s[ 0 ] == '(' )
		{
			flushFields ();
			tuneNo = std::atoi ( s.substr ( 2, s.size () > 3 ? s.size () - 3 : 0 ).c_str () );
			continue;
		}

		if ( s.rfind ( "         ", 0 ) == 0 )		// nine (9) spaces: continuation
		{
			if ( ! fields.empty () )
				pendingLines.push_back ( s );
			continue;
		}

		if ( s.size () < 9 )
			continue;

		mergeField ();

		auto	name = s.substr ( 0, 9 );
		for ( auto& c : name )
			c = char ( std::toupper ( (unsigned char)c ) );

		name = name.substr ( 0, name.size () - 2 );
		name.erase ( 0, name.find_first_not_of ( ' ' ) );

		fields.emplace_back ( name, s.substr ( 9 ) );
	}
	flushFields ();
}
//-----------------------------------------------------------------------------

int main ()
{
	const auto	root = fs::path ( STIL_TEST_ROOT );

	// Machine-specific HVSC location, like the other Tests/ tools
	fs::path	hvscRoot;
	{
		std::istringstream	f ( [ & ] { std::ifstream in ( root / "Tests" / "data-roots.txt" ); std::stringstream s; s << in.rdbuf (); return s.str (); } () );
		std::string	line;

		while ( std::getline ( f, line ) )
			if ( const auto eq = line.find ( '=' ); line.rfind ( "$HVSC$", 0 ) == 0 && eq != std::string::npos )
			{
				auto	dir = line.substr ( eq + 1 );
				dir.erase ( 0, dir.find_first_not_of ( " \t" ) );
				while ( ! dir.empty () && ( dir.back () == ' ' || dir.back () == '\r' ) )
					dir.pop_back ();

				hvscRoot = fs::path ( dir ).is_absolute () ? fs::path ( dir ) : root / dir;
			}
	}

	if ( hvscRoot.empty () || ! fs::exists ( hvscRoot / "DOCUMENTS" / "STIL.txt" ) )
	{
		std::cout << "No STIL.txt found; set $HVSC$ in Tests/data-roots.txt\n";
		return 1;
	}

	std::vector<Comment>	comments;
	parseStil ( readLines ( hvscRoot / "DOCUMENTS" / "STIL.txt" ), comments );

	// An addendum comment overrides the STIL comment of the same tune, like
	// the app's merge does
	std::vector<Comment>	addendum;
	parseStil ( readLines ( root / "Data" / "Databases" / "STIL-addendum.txt" ), addendum );

	for ( const auto& add : addendum )
	{
		auto	match = std::find_if ( comments.begin (), comments.end (), [ & ] ( const Comment& c ) { return c.file == add.file && c.tune == add.tune; } );

		if ( match != comments.end () )
			match->body = add.body;
		else
			comments.push_back ( add );
	}

	// Watched entries: the awkward STIL comments, printed in full
	const char*	watched[] = { "128_Byte_Blues_BASIC", "Daglish_Ben/Last_Ninja.sid",
							  "Galway_Martin/Arkanoid.sid", "Follin_Tim/Bionic_Commando.sid" };

	size_t	quotes = 0, stillDelimited = 0, innerQuoted = 0;
	std::vector<std::string>	delimitedCases, innerCases;

	for ( const auto& c : comments )
	{
		const auto	segs = stilq::splitCommentQuotes ( std::string { lime::str::trim ( c.body ) } );

		for ( const auto* want : watched )
			if ( c.file.find ( want ) != std::string::npos && c.tune == 0 )
			{
				std::cout << "======== " << c.file << " ========\n";
				for ( const auto& seg : segs )
					std::cout << "[" << seg.type << ( seg.speaker.empty () ? "" : "/" + seg.speaker ) << "] " << seg.text << "\n\n";
			}

		for ( const auto& seg : segs )
		{
			if ( seg.type != "QUOTE" )
				continue;

			++quotes;
			const auto&	t = seg.text;

			if ( t.size () >= 2 && t.front () == '"' && t.back () == '"' )
			{
				++stillDelimited;
				delimitedCases.push_back ( c.file + ": " + t );
			}
			else if ( t.find ( '"' ) != std::string::npos )
			{
				++innerQuoted;
				if ( innerCases.size () < 20 )
					innerCases.push_back ( c.file + ": " + t );
			}
		}
	}

	std::cout << "comments scanned:   " << comments.size () << "\n";
	std::cout << "quotes detected:    " << quotes << "\n";
	std::cout << "still delimited:    " << stillDelimited << "\n";
	std::cout << "with inner quotes:  " << innerQuoted << "\n\n";

	for ( const auto& s : delimitedCases )
		std::cout << "DELIMITED: " << s << "\n\n";
	for ( const auto& s : innerCases )
		std::cout << "inner: " << s << "\n\n";

	const auto	ok = stillDelimited == 0 && quotes >= 2000;
	std::cout << ( ok ? "PASS" : "FAIL" ) << "\n";
	return ok ? 0 : 1;
}
//-----------------------------------------------------------------------------
