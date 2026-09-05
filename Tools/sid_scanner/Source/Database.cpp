#include <algorithm>
#include <format>

#include "Database.h"

#include "libSidplayEZ/src/EZ/shared-config.h"

#include "ultra-shared/Helpers/FileUtils.h"

#include "Database/SonglengthsParser.h"
#include "sid_scanner.h"

//-----------------------------------------------------------------------------

// Format milliseconds as "M:SS", appending the ".fff" fraction only when it's non-zero
static std::string formatTime ( const uint32_t ms )
{
	auto	str = std::format ( "{}:{:02}", ms / 60'000u, ( ms / 1000u ) % 60u );

	if ( ms % 1000u )
		str += std::format ( ".{:03}", ms % 1000u );

	return str;
}
//-----------------------------------------------------------------------------

void Database::attach ()
{
	// Get main version-number
	hvscVersion = -1;

	const auto	text = hvscsource::loadText ( "DOCUMENTS/HVSC.txt" );
	if ( text.isEmpty () )
		return;

	const auto	fileStrings = juce::StringArray::fromLines ( text );

	//
	// Get version number
	//
	for ( auto i = 0; i < fileStrings.size () && hvscVersion <= 0; ++i )
		if ( auto pos = fileStrings.getReference ( i ).indexOf ( "Release " ); pos >= 0 )
			hvscVersion = fileStrings.getReference ( i ).getTrailingIntValue ();

	if ( hvscVersion < minHVSCVersion )
		return;

	if ( ! loadLengths () )
		return;

	loadLUFS ();
	loadStarts ();
	loadSettings ();
	loadWriteRates ();
	loadDigiHints ();
	loadBits ( "SID_Filter.txt", &entry::filterUsed );
	loadBits ( "SID_Digi.txt", &entry::digiUsed );
	loadBits ( "SID_Loop.txt", &entry::looped );
}
//-----------------------------------------------------------------------------

bool Database::loadLengths ()
{
	auto loadEntries = [ this ] ( const juce::String& content, const bool skipExisting )
	{
		parseSonglengthsText ( content, [ this, skipExisting ] ( const juce::String& path, const std::vector<uint32_t>& timesMs, const juce::String& md5 )
		{
			// Keys carry no ".sid"
			const auto	name = path.dropLastCharacters ( 4 ).toStdString ();

			// The real collection always wins over addendum entries
			if ( skipExisting && db.contains ( name ) )
				return;

			auto&	ent = db[ name ];

			ent.lengths = timesMs;
			ent.md5 = md5.toStdString ();
		} );
	};

	loadEntries ( hvscsource::loadText ( "DOCUMENTS/Songlengths.md5" ), false );

	// Tunes the HVSC doesn't include yet (the Exotic-tunes mirror)
	loadEntries ( dataRoot ().getChildFile ( "Databases/Songlengths-addendum.md5" ).loadFileAsString (), true );

	// Lengths corrected along with a rip's bytes replace the HVSC's entry
	for ( const auto& [ path, times ] : tunepatches::lengths () )
		if ( const auto it = db.find ( path.dropLastCharacters ( 4 ).toStdString () ); it != db.end () )
			it->second.lengths = parseSonglengthTimes ( times );

	return ! db.empty ();
}
//-----------------------------------------------------------------------------

template <typename ParseValues>
bool Database::loadFile ( const char* filename, ParseValues parseValues )
{
	// Open file
	auto	file = scannerDataFile ( filename );
	if ( ! file.existsAsFile () )
		return false;

	// Load file into separate lines
	juce::StringArray	fileStrings;
	file.readLines ( fileStrings );

	for ( const auto& s : fileStrings )
	{
		auto	name = s.upToFirstOccurrenceOf ( "=", false, false ).toStdString ();

		parseValues ( db[ name ], s.fromFirstOccurrenceOf ( "=", false, false ) );
	}

	return true;
}
//-----------------------------------------------------------------------------

template <typename FormatValues>
void Database::saveFile ( const char* filename, FormatValues formatValues )
{
	if ( db.empty () )
		return;

	// Open file
	auto	file = scannerDataFile ( filename );

	// Write new map to string
	std::string	output;

	output.reserve ( 10'000'000 );	// 10 MB should be fine

	for ( const auto& [ name, ent ] : db )
	{
		const auto	line = formatValues ( ent );

		// No data for this entry
		if ( line.empty () )
			continue;

		output += name + "=" + line + "\r\n";
	}

	fileutils::replaceFile ( file, output.data (), output.size () );
}
//-----------------------------------------------------------------------------

bool Database::loadLUFS ()
{
	return loadFile ( "SID_LUFS.txt", [] ( entry& ent, const juce::String& values )
	{
		auto	lufsArr = juce::StringArray::fromTokens ( values, " ", "" );
		lufsArr.removeEmptyStrings ();

		// Each token is "loudness/midLoudness"; a bare loudness (pre-midband file)
		// loads as -96 = no midband data
		auto&	vec = ent.loudness;
		auto&	mid = ent.midLoudness;
		vec.clear ();
		mid.clear ();
		vec.reserve ( lufsArr.size () );
		mid.reserve ( lufsArr.size () );

		for ( const auto& lufs : lufsArr )
		{
			vec.emplace_back ( lufs.getFloatValue () );

			const auto	slash = lufs.indexOfChar ( '/' );
			mid.emplace_back ( slash >= 0 ? lufs.substring ( slash + 1 ).getFloatValue () : -96.0f );
		}

		vec.shrink_to_fit ();
		mid.shrink_to_fit ();
	} );
}
//-----------------------------------------------------------------------------

bool Database::loadSettings ()
{
	return loadFile ( "SID_Settings.txt", [] ( entry& ent, const juce::String& values )
	{
		auto	tokens = juce::StringArray::fromTokens ( values, " ", "" );
		tokens.removeEmptyStrings ();

		auto&	vec = ent.settings;
		vec.clear ();
		vec.reserve ( size_t ( tokens.size () ) );

		// "-" pads a not-yet-recorded slot
		for ( const auto& token : tokens )
			vec.emplace_back ( token == "-" ? std::string () : token.toStdString () );

		vec.shrink_to_fit ();
	} );
}
//-----------------------------------------------------------------------------

bool Database::loadWriteRates ()
{
	return loadFile ( "SID_WriteRates.txt", [] ( entry& ent, const juce::String& values )
	{
		auto	tokens = juce::StringArray::fromTokens ( values, " ", "" );
		tokens.removeEmptyStrings ();

		auto&	vec = ent.writeRates;
		vec.clear ();
		vec.reserve ( size_t ( tokens.size () ) );

		// "." pads a subtune never scanned in unknown mode ("-" = scanned clean)
		for ( const auto& token : tokens )
			vec.emplace_back ( token == "." ? std::string () : token.toStdString () );

		vec.shrink_to_fit ();
	} );
}
//-----------------------------------------------------------------------------

bool Database::loadDigiHints ()
{
	return loadFile ( "SID_DigiHints.txt", [] ( entry& ent, const juce::String& values )
	{
		// Hints carry spaces ("output (resynthesis)"), subtunes split on '|'
		auto	tokens = juce::StringArray::fromTokens ( values, "|", "" );

		auto&	vec = ent.digiHints;
		vec.clear ();
		vec.reserve ( size_t ( tokens.size () ) );

		// "-" pads a subtune without a hint
		for ( const auto& token : tokens )
		{
			const auto	hint = token.trim ();
			vec.emplace_back ( hint == "-" ? std::string () : hint.toStdString () );
		}

		vec.shrink_to_fit ();
	} );
}
//-----------------------------------------------------------------------------

bool Database::loadStarts ()
{
	return parseSonglengths ( songdelaysFile (), [ this ] ( const juce::String& path, const std::vector<uint32_t>& timesMs, const juce::String& )
	{
		// Keys carry no ".sid"
		if ( auto it = db.find ( path.dropLastCharacters ( 4 ).toStdString () ); it != db.end () )
			it->second.startOffset = timesMs;
	} );
}
//-----------------------------------------------------------------------------

bool Database::loadBits ( const char* filename, std::vector<bool> entry::* member )
{
	return loadFile ( filename, [ member ] ( entry& ent, const juce::String& values )
	{
		const auto	bitStr = values.trim ().toStdString ();

		// Convert string bit-string to vector of bools
		auto&	vec = ent.*member;
		vec.clear ();
		vec.reserve ( bitStr.size () );

		for ( const auto bit : bitStr )
			vec.emplace_back ( bit == '1' );

		vec.shrink_to_fit ();
	} );
}
//-----------------------------------------------------------------------------

void Database::addEntry ( const std::string& name, const unsigned int songNo, const float lufs, const float midLoudness, const bool filterUsed, const bool digiUsed, const bool looped, const uint32_t startMs, const std::string& settingsHash, const std::string& writeRates, const std::string& digiHint )
{
	// Update loudness entry
	if ( lufs > -96.0f && std::abs ( lufs ) > 0.01f )
	{
		auto&	vec = db[ name ].loudness;
		if ( vec.size () < songNo )
			vec.resize ( songNo, 0.0f );

		vec[ songNo - 1 ] = lufs;

		auto&	mid = db[ name ].midLoudness;
		if ( mid.size () < songNo )
			mid.resize ( songNo, -96.0f );

		mid[ songNo - 1 ] = midLoudness;
	}

	// Update filter entry
	{
		auto&	vec = db[ name ].filterUsed;
		if ( vec.size () < songNo )
			vec.resize ( songNo, true );

		vec[ songNo - 1 ] = filterUsed;
	}

	// Update digi entry
	{
		auto&	vec = db[ name ].digiUsed;
		if ( vec.size () < songNo )
			vec.resize ( songNo, false );

		vec[ songNo - 1 ] = digiUsed;
	}

	// Update loop entry
	{
		auto&	vec = db[ name ].looped;
		if ( vec.size () < songNo )
			vec.resize ( songNo, true );

		vec[ songNo - 1 ] = looped;
	}

	// Update start-offset entry
	{
		auto&	vec = db[ name ].startOffset;
		if ( vec.size () < songNo )
			vec.resize ( songNo, 0 );

		vec[ songNo - 1 ] = startMs;
	}

	// Update settings-fingerprint entry
	{
		auto&	vec = db[ name ].settings;
		if ( vec.size () < songNo )
			vec.resize ( songNo );

		vec[ songNo - 1 ] = settingsHash;
	}

	// Update write-rates entry
	{
		auto&	vec = db[ name ].writeRates;
		if ( vec.size () < songNo )
			vec.resize ( songNo );

		vec[ songNo - 1 ] = writeRates;
	}

	// Update digi-hint entry
	{
		auto&	vec = db[ name ].digiHints;
		if ( vec.size () < songNo )
			vec.resize ( songNo );

		vec[ songNo - 1 ] = digiHint;
	}
}
//-----------------------------------------------------------------------------

void Database::saveLUFS ()
{
	saveFile ( "SID_LUFS.txt", [] ( const entry& ent )
	{
		std::string	line;

		for ( auto lcnt = 0u; const auto lufs : ent.loudness )
		{
			const auto	mid = lcnt < ent.midLoudness.size () ? ent.midLoudness[ lcnt ] : -96.0f;
			line += std::format ( "{:.1f}/{:.1f}", lufs, mid ) + ( ( ++lcnt == ent.loudness.size () ) ? "" : " " );
		}

		return line;
	} );
}
//-----------------------------------------------------------------------------

void Database::saveSettings ()
{
	saveFile ( "SID_Settings.txt", [] ( const entry& ent )
	{
		std::string	line;

		// "-" pads a not-yet-recorded slot
		for ( auto cnt = 0u; const auto& hash : ent.settings )
			line += ( hash.empty () ? "-" : hash ) + ( ( ++cnt == ent.settings.size () ) ? "" : " " );

		return line;
	} );
}
//-----------------------------------------------------------------------------

void Database::saveWriteRates ()
{
	saveFile ( "SID_WriteRates.txt", [] ( const entry& ent )
	{
		// Only tunes with at least one unknown-mode scan get a line; "-"
		// records a clean subtune, so proven cleanliness persists
		if ( std::ranges::all_of ( ent.writeRates, [] ( const auto& rates ) { return rates.empty (); } ) )
			return std::string ();

		std::string	line;

		// "." pads a subtune never scanned in unknown mode
		for ( auto cnt = 0u; const auto& rates : ent.writeRates )
			line += ( rates.empty () ? "." : rates ) + ( ( ++cnt == ent.writeRates.size () ) ? "" : " " );

		return line;
	} );
}
//-----------------------------------------------------------------------------

void Database::saveDigiHints ()
{
	if ( db.empty () )
		return;

	// Not the saveFile skeleton: the covered check needs the entry's name
	std::string	output;
	output.reserve ( 100'000 );

	for ( const auto& [ name, ent ] : db )
	{
		if ( std::ranges::all_of ( ent.digiHints, [] ( const auto& hint ) { return hint.empty (); } ) )
			continue;

		// A tune covered by digi-tunes.csv in the meantime is handled, its
		// hint was the to-do item
		if ( sharedConfig && sharedConfig->digiSelector.getDigi ( name.c_str (), ".sid", {} ).covered )
			continue;

		// Hints carry spaces, subtunes join on '|'; "-" pads a subtune
		// without a hint
		std::string	line;
		for ( auto cnt = 0u; const auto& hint : ent.digiHints )
			line += ( hint.empty () ? "-" : hint ) + ( ( ++cnt == ent.digiHints.size () ) ? "" : "|" );

		output += name + "=" + line + "\r\n";
	}

	fileutils::replaceFile ( scannerDataFile ( "SID_DigiHints.txt" ), output.data (), output.size () );
}
//-----------------------------------------------------------------------------

void Database::saveBits ( const char* filename, std::vector<bool> entry::* member )
{
	saveFile ( filename, [ member ] ( const entry& ent )
	{
		std::string	line;
		line.reserve ( ( ent.*member ).size () );

		for ( const auto bit : ent.*member )
			line += bit ? '1' : '0';

		return line;
	} );
}
//-----------------------------------------------------------------------------

void Database::saveFilterUsed ()
{
	saveBits ( "SID_Filter.txt", &entry::filterUsed );
}
//-----------------------------------------------------------------------------

void Database::saveDigiUsed ()
{
	saveBits ( "SID_Digi.txt", &entry::digiUsed );
}
//-----------------------------------------------------------------------------

void Database::saveLooped ()
{
	saveBits ( "SID_Loop.txt", &entry::looped );
}
//-----------------------------------------------------------------------------

void Database::saveStarts ()
{
	// Songlengths.md5 format (comment with filename, then md5 and the delays), so
	// the shared parser reads it back and the file looks familiar
	std::string	output = "[Database]\r\n";

	for ( const auto& [ name, ent ] : db )
	{
		// Only tunes where at least one subtune has a start offset get an entry
		if ( std::ranges::all_of ( ent.startOffset, [] ( const auto ms ) { return ms == 0; } ) )
			continue;

		output += "; " + name + ".sid\r\n" + ent.md5 + "=";

		for ( auto cnt = 0u; const auto ms : ent.startOffset )
			output += formatTime ( ms ) + ( ( ++cnt == ent.startOffset.size () ) ? "" : " " );

		output += "\r\n";
	}

	fileutils::replaceFile ( songdelaysFile (), output.data (), output.size () );
}
//-----------------------------------------------------------------------------
