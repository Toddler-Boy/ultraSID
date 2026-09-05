#include <JuceHeader.h>

#include <print>
#include <string>
#include <unordered_map>
#include <vector>

#include "DatabaseBuilder.h"

#include "ultra-shared/Helpers/FileUtils.h"

#include "Database/SonglengthsParser.h"
#include "Database/TuneNames.h"
#include "Database/uSIDFormat.h"
#include "sid_scanner.h"

//-----------------------------------------------------------------------------

// Data structure to hold SID metadata and associated information
struct SidEntry
{
	juce::String	file;

	std::string		name;
	std::string		author;
	std::string		release;

	std::vector<int>	len;		// Song lengths in milliseconds
	std::vector<float>	loud;		// LUFS loudness readings
	std::vector<float>	mid;		// midband loudness readings
	juce::String		filter;		// "1" or "0" per song
	juce::String		digi;		// "1" or "0" per song
	juce::String		loop;		// "1" or "0" per song

	uint16_t	version = 0;
	uint16_t	play = 0;
	uint16_t	songs = 0;
	uint16_t	start = 0;
	uint16_t	flags = 0;
};
//-----------------------------------------------------------------------------

static std::string trimStdString ( const std::string& str )
{
	auto isWhitespaceOrNull = [] ( unsigned char ch ) {
		return std::isspace ( ch ) || ch == '\0';
	};

	auto first = std::find_if_not ( str.begin (), str.end (), isWhitespaceOrNull );
	if ( first == str.end () )
		return "";

	auto last = std::find_if_not ( str.rbegin (), str.rend (), isWhitespaceOrNull ).base ();
	return std::string ( first, last );
}
//-----------------------------------------------------------------------------

static bool startsWithIgnoreCase ( const std::string& str, const std::string& prefix )
{
	if ( str.length () < prefix.length () )
		return false;

	return std::equal ( prefix.begin (), prefix.end (), str.begin (),
		[] ( unsigned char a, unsigned char b ) {
		return std::tolower ( a ) == std::tolower ( b );
	} );
}
//-----------------------------------------------------------------------------

static void appendPascalString ( juce::MemoryBlock& block, const std::string& text )
{
	const auto	length = uint8_t ( std::min ( text.length (), size_t ( 255 ) ) );

	block.append ( &length, 1 );
	block.append ( text.data (), length );
}
//-----------------------------------------------------------------------------

static std::unordered_map<juce::String, SidEntry> loadSonglengths ()
{
	std::unordered_map<juce::String, SidEntry>	database;

	auto loadEntries = [ &database ] ( const juce::String& content, const bool skipExisting )
	{
		parseSonglengthsText ( content, [ &database, skipExisting ] ( const juce::String& path, const std::vector<uint32_t>& timesMs, const juce::String& )
		{
			// Keys carry no ".sid"
			const auto	name = path.dropLastCharacters ( 4 );

			// The real collection always wins over addendum entries
			if ( skipExisting && database.contains ( name ) )
				return;

			SidEntry	entry;
			entry.file = name;
			entry.len.assign ( timesMs.begin (), timesMs.end () );

			database[ name ] = std::move ( entry );
		} );
	};

	loadEntries ( hvscsource::loadText ( "DOCUMENTS/Songlengths.md5" ), false );

	// Tunes the HVSC doesn't include yet (the Exotic-tunes mirror)
	loadEntries ( dataRoot ().getChildFile ( "Databases/Songlengths-addendum.md5" ).loadFileAsString (), true );

	// Lengths corrected along with a rip's bytes replace the HVSC's entry
	for ( const auto& [ path, times ] : tunepatches::lengths () )
	{
		if ( const auto it = database.find ( path.dropLastCharacters ( 4 ) ); it != database.end () )
		{
			const auto	timesMs = parseSonglengthTimes ( times );
			it->second.len.assign ( timesMs.begin (), timesMs.end () );
		}
	}

	return database;
}
//-----------------------------------------------------------------------------

static void loadLUFS ( std::unordered_map<juce::String, SidEntry>& database )
{
	auto	file = scannerDataFile ( "SID_LUFS.txt" );
	if ( ! file.existsAsFile () )
		return;

	auto	lines = juce::StringArray::fromLines ( file.loadFileAsString () );
	lines.trim ();
	lines.removeEmptyStrings ();

	for ( auto& line : lines )
	{
		auto	eqIndex = line.indexOfChar ( '=' );
		if ( eqIndex == -1 )
			continue;

		auto	key = line.substring ( 0, eqIndex );

		if ( ! database.contains ( key ) )
			continue;

		auto	valuesPart = line.substring ( eqIndex + 1 );
		auto	valTokens = juce::StringArray::fromTokens ( valuesPart, " ", "" );

		// Each token is "loudness/midLoudness"
		for ( auto& v : valTokens )
		{
			database[ key ].loud.push_back ( v.getFloatValue () );

			const auto	slash = v.indexOfChar ( '/' );
			database[ key ].mid.push_back ( slash >= 0 ? v.substring ( slash + 1 ).getFloatValue () : -96.0f );
		}
	}
}
//-----------------------------------------------------------------------------

static void loadBits ( std::unordered_map<juce::String, SidEntry>& database, const juce::String& filename, const juce::String& dst )
{
	auto	file = scannerDataFile ( filename );
	if ( ! file.existsAsFile () )
		return;

	auto	lines = juce::StringArray::fromLines ( file.loadFileAsString () );
	lines.trim ();
	lines.removeEmptyStrings ();

	for ( auto& line : lines )
	{
		auto	eqIndex = line.indexOfChar ( '=' );
		if ( eqIndex == -1 )
			continue;

		auto	key = line.substring ( 0, eqIndex );

		if ( ! database.contains ( key ) )
			continue;

		auto	values = line.substring ( eqIndex + 1 );

		if ( dst == "filter" )
			database[ key ].filter = values;
		else if ( dst == "digi" )
			database[ key ].digi = values;
		else if ( dst == "loop" )
			database[ key ].loop = values;
	}
}
//-----------------------------------------------------------------------------

static int getHVSCVersion ()
{
	const auto	text = hvscsource::loadText ( "DOCUMENTS/HVSC.txt" );
	if ( text.isEmpty () )
		return 0;

	auto	lines = juce::StringArray::fromLines ( text );
	lines.trim ();

	for ( auto& line : lines )
		if ( line.startsWithIgnoreCase ( "Release " ) )
			return line.substring ( 8 ).getIntValue ();

	return 0;
}
//-----------------------------------------------------------------------------

int buildDatabase ( std::atomic<float>* progress )
{
	const auto	hvscRoot = ultraSIDHVSCPath ();
	if ( ! hvscsource::setRoot ( hvscRoot ) )
	{
		std::println ( "ultraSID's HVSC path isn't valid: {}", hvscRoot.getFullPathName ().toStdString () );
		Z_ERR ( "db build: HVSC path isn't valid: " << hvscRoot.getFullPathName () );
		return 20;
	}
	tunepatches::load ( dataRoot ().getChildFile ( "Databases/tune-patches.txt" ).loadFileAsString () );

	// The Exotic-tunes addendum is part of the corpus, a db built without it
	// would silently lose those tunes
	if ( ! dataRoot ().getChildFile ( "Databases/Songlengths-addendum.md5" ).existsAsFile () )
	{
		std::println ( "Can't find ultraSID's Data/Databases/Songlengths-addendum.md5" );
		Z_ERR ( "db build: Songlengths-addendum.md5 missing" );
		return 20;
	}

	auto	database = loadSonglengths ();
	loadLUFS ( database );
	loadBits ( database, "SID_Filter.txt", "filter" );
	loadBits ( database, "SID_Digi.txt", "digi" );
	loadBits ( database, "SID_Loop.txt", "loop" );

	auto	keys = juce::StringArray ();
	for ( const auto& pair : database )
		keys.add ( pair.first );
	keys.sort ( false );

	auto	hvscVersion = getHVSCVersion ();
	auto	hvscEntryCnt = database.size ();

	std::println ( "Converting HVSC version {}", hvscVersion );
	std::println ( "{} entries", hvscEntryCnt );

	std::vector<SidEntry>	processedEntries;
	auto	cnt = 0;

	// Parse individual SID files metadata headers
	for ( auto& key : keys )
	{
		const auto	spec = resolveTuneSpec ( key );
		if ( spec.isEmpty () )
			continue;

		// The whole file through the scanner's loader, so header patches land in the db
		std::vector<uint8_t>	sidData;

		scannerLoadBytes ( spec.toRawUTF8 (), sidData );

		if ( sidData.size () < 0x78 )
			continue;

		const auto	bytes = sidData.data ();

		auto&	value = database[ key ];

		value.version = ( bytes[ 0x4 ] << 8 ) | bytes[ 0x5 ];
		value.play = ( bytes[ 0x0C ] << 8 ) | bytes[ 0x0D ];
		value.songs = ( bytes[ 0x0E ] << 8 ) | bytes[ 0x0F ];
		value.start = ( bytes[ 0x10 ] << 8 ) | bytes[ 0x11 ];

		// Unpack Name (32 Bytes, strip padding/spaces)
		value.name = std::string ( reinterpret_cast<const char*>( &bytes[ 0x16 ] ), 32 );
		value.author = std::string ( reinterpret_cast<const char*>( &bytes[ 0x36 ] ), 32 );
		value.release = std::string ( reinterpret_cast<const char*>( &bytes[ 0x56 ] ), 32 );

		value.name = trimStdString ( value.name );
		value.author = trimStdString ( value.author );
		value.release = trimStdString ( value.release );

		// Moves "The " (4 characters) to the end: "Neverending Story, The"
		if ( startsWithIgnoreCase ( value.name, "The " ) )
			value.name = value.name.substr ( 4 ) + ", " + value.name.substr ( 0, 3 );

		value.flags = ( value.version >= 2 ) ? ( ( bytes[ 0x76 ] << 8 ) | bytes[ 0x77 ] ) : 0;

		processedEntries.push_back ( value );

		cnt++;
		if ( progress && ( cnt % 10 ) == 0 )
			progress->store ( float ( cnt ) / float ( keys.size () ) );
	}
	std::println ( "{} entries processed", cnt );

	// Placeholder ("<?>") titles and same-named tunes within one folder are
	// impossible to tell apart, those show their filename instead
	{
		std::unordered_map<std::string, int>	nameCounts;

		auto	folderAndName = [] ( const SidEntry& e )
		{
			const auto	file = e.file.toStdString ();
			return file.substr ( 0, file.rfind ( '/' ) + 1 ) + tunenames::folded ( e.name );
		};

		for ( const auto& e : processedEntries )
			if ( ! tunenames::isPlaceholder ( e.name ) )
				++nameCounts[ folderAndName ( e ) ];

		auto	renamed = 0;

		for ( auto& e : processedEntries )
			if ( tunenames::isPlaceholder ( e.name ) || nameCounts[ folderAndName ( e ) ] > 1 )
			{
				e.name = tunenames::stemName ( e.file.toStdString () );
				++renamed;
			}

		std::println ( "{} placeholder/duplicate names use their filename", renamed );
	}

	std::println ( "Building output file" );

	// Write out highly optimized raw binary blocks using JUCE memory layouts
	juce::MemoryBlock	packedPayload;

	// Little Endian Count (uint32_t)
	auto	totalCount = uint32_t ( processedEntries.size () );
	packedPayload.append ( &totalCount, sizeof ( totalCount ) );

	for ( auto& entry : processedEntries )
	{
		appendPascalString ( packedPayload, entry.file.toStdString () );

		appendPascalString ( packedPayload, entry.name );
		appendPascalString ( packedPayload, entry.author );
		appendPascalString ( packedPayload, entry.release );

		packedPayload.append ( &entry.flags, sizeof ( entry.flags ) );
		packedPayload.append ( &entry.start, sizeof ( entry.start ) );
		packedPayload.append ( &entry.songs, sizeof ( entry.songs ) );

		auto	filter = entry.filter.paddedRight ( '1', entry.songs );
		auto	digi = entry.digi.paddedRight ( entry.play ? '0' : '1', entry.songs );
		auto	loop = entry.loop.paddedRight ( '1', entry.songs );		// unmeasured counts as looping

		for ( auto idx = 0; idx < entry.songs; ++idx )
		{
			// Extract single character indices
			auto	filterBit = filter.substring ( idx, idx + 1 ).getIntValue ();
			auto	digiBit = digi.substring ( idx, idx + 1 ).getIntValue ();
			auto	loopBit = loop.substring ( idx, idx + 1 ).getIntValue ();

			auto	loudnessVal = ( idx < entry.loud.size () ) ? entry.loud[ idx ] : 0.0f;

			auto	outWordLE = usid::packProperties ( loudnessVal, filterBit != 0, digiBit != 0, loopBit == 0 );
			packedPayload.append ( &outWordLE, sizeof ( outWordLE ) );

			// -96 in the text file = no midband data, packs as the unmeasured all-zero word
			auto	midVal = ( idx < entry.mid.size () && entry.mid[ idx ] > -95.95f ) ? entry.mid[ idx ] : 0.0f;

			auto	midWordLE = usid::packMidLoudness ( midVal );
			packedPayload.append ( &midWordLE, sizeof ( midWordLE ) );
		}
	}

	std::println ( "Writing output file" );

	auto	payloadLength = static_cast<uint32_t>( packedPayload.getSize () );

	// final Target Construction Container
	juce::MemoryBlock finalDatabase;
	finalDatabase.append ( usid::magic, sizeof ( usid::magic ) );

	auto	hvscVerByte = static_cast<uint8_t>( hvscVersion );

	finalDatabase.append ( &hvscVerByte, sizeof ( hvscVerByte ) );
	finalDatabase.append ( &payloadLength, sizeof ( payloadLength ) );
	finalDatabase.append ( packedPayload.getData (), packedPayload.getSize () );

	// Straight to where ultraSID loads it from
	auto	outputFile = dataRoot ().getChildFile ( "ultraSID.db" );

	if ( fileutils::replaceFile ( outputFile, finalDatabase.getData (), finalDatabase.getSize () ) )
	{
		if ( progress )
			progress->store ( 1.0f );

		std::println ( "Done. {} entries written to {}", cnt, outputFile.getFullPathName ().toStdString () );
		Z_LOG ( "db " << hvscVersion << " built: " << cnt << " entries -> " << outputFile.getFullPathName () );
		return 0;
	}

	std::println ( "Error writing output file." );
	return 20;
}
//-----------------------------------------------------------------------------
