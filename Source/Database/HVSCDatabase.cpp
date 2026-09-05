#include <JuceHeader.h>

#include "HVSCDatabase.h"

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Config/DataSource.h"

#include "Config/HVSCSource.h"
#include "Config/TunePatches.h"
#include "Database/SonglengthsParser.h"

//-----------------------------------------------------------------------------

HVSC_database::HVSC_database ()
	: juce::Thread ( "HSVC database loader" )
{
}
//-----------------------------------------------------------------------------

HVSC_database::~HVSC_database ()
{
	stopThread ( -1 );
}
//-----------------------------------------------------------------------------

void HVSC_database::attach ()
{
	stopThread ( -1 );

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
}
//-----------------------------------------------------------------------------

void HVSC_database::load ( std::function<void ()> _callback )
{
	if ( hvscVersion <= 0 )
		return;

	stopThread ( -1 );
	callback = std::move ( _callback );
	startThread ( juce::Thread::Priority::low );
}
//-----------------------------------------------------------------------------

void HVSC_database::run ()
{
	{
		const juce::ScopedLock	sl ( dbLock );
		errorCode = "scanning";
	}

//	juce::Thread::sleep ( 5000 );

	//
	// Load HVSC databases
	//
	loadLengths ();
	loadStarts ();
	loadSTIL ();
	loadBugs ();

	if ( threadShouldExit () )
		return;

	{
		const juce::ScopedLock	sl ( dbLock );

		if ( lengthDB.size () < 60'000 )
			errorCode = "Length database incomplete";

		if ( stilDB.size () < 18'000 )
			errorCode = "STIL incomplete";

		if ( errorCode == "scanning" )
			errorCode = "";
	}

	juce::MessageManager::callAsync ( callback );
}
//-----------------------------------------------------------------------------

std::optional<HVSC_database::STIL_block> HVSC_database::getSTILEntry ( const std::string& name, const int tune /*= 0 */ )
{
	const auto	key = lime::str::toLower ( name );

	const juce::ScopedLock	sl ( dbLock );

	if ( stilDB.contains ( key ) && stilDB[ key ].contains ( tune ) )
		return stilDB[ key ][ tune ];

	return {};
}
//-----------------------------------------------------------------------------

uint32_t HVSC_database::lookupMs ( const msMap& db, const std::string_view name, const int tune ) const
{
	if ( tune < 1 || tune > 256 )
		return 0;

	// The lookup key is the path past the location marker ("$HVSC$" / "$USER$")
	const juce::ScopedLock	sl ( dbLock );

	auto	it = db.find ( lime::str::toLower ( std::string ( filepaths::stripLocationMarker ( name ) ) ) );
	if ( it == db.end () )
		return 0;

	auto&	vec = it->second;
	if ( tune > int ( vec.size () ) )
		return 0;

	return vec[ size_t ( tune ) - 1 ];
}
//-----------------------------------------------------------------------------

uint32_t HVSC_database::getLengthMs ( const std::string_view name, const int tune ) const
{
	jassert ( tune >= 1 && tune <= 256 );

	return lookupMs ( lengthDB, name, tune );
}
//-----------------------------------------------------------------------------

void HVSC_database::loadLengths ()
{
	const juce::ScopedLock	sl ( dbLock );

	lengthDB = {};

	auto perEntry = [ this ] ( const juce::String& name, const std::vector<uint32_t>& timesMs, const juce::String& )
	{
		// Keys folded like Database::entry::lowerFile, so lookups can't
		// depend on the collection's filename casing
		auto&	ent = lengthDB[ lime::str::toLower ( name.toStdString () ) ];

		// When both files carry a tune, the first one loaded (the real HVSC) wins
		if ( ent.empty () )
			ent = timesMs;
	};

	parseSonglengthsText ( hvscsource::loadText ( "DOCUMENTS/Songlengths.md5" ), perEntry );

	// Lengths for the tunes the HVSC doesn't include yet (the Exotic-tunes mirror)
	parseSonglengthsText ( datasource::loadText ( "Databases/Songlengths-addendum.md5" ), perEntry );

	// Lengths corrected along with a rip's bytes replace the HVSC's entry
	for ( const auto& [ path, times ] : tunepatches::lengths () )
		lengthDB[ lime::str::toLower ( path.toStdString () ) ] = parseSonglengthTimes ( times );
}
//-----------------------------------------------------------------------------

void HVSC_database::loadStarts ()
{
	const juce::ScopedLock	sl ( dbLock );

	startDB.clear ();

	// Written by sid_scanner and shipped as-is, in the Songlengths format; only
	// tunes with a delayed start have an entry. Keys are folded exactly like
	// Database::entry::lowerFile, so its lookups need no folding of their own
	parseSonglengthsText ( datasource::loadText ( "Databases/Songdelays.md5" ), [ this ] ( const juce::String& name, const std::vector<uint32_t>& timesMs, const juce::String& )
	{
		startDB[ lime::str::toLower ( name.toStdString () ) ] = timesMs;
	} );
}
//-----------------------------------------------------------------------------

uint32_t HVSC_database::getStartMs ( const std::string_view name, const int tune ) const
{
	// A missing start entry is normal, no assert here
	return lookupMs ( startDB, name, tune );
}
//-----------------------------------------------------------------------------

// A continuation line that opens a quotation gets its own line
static bool startsLongQuote ( const juce::String& s )
{
	if ( ! s.startsWithChar ( '"' ) )
		return false;

	const auto	pos = s.indexOfChar ( 1, '"' );
	return pos == -1 || pos >= 20;
}
//-----------------------------------------------------------------------------

// Rebuild a COMMENT from its wrapped source lines: a break the wrap width did
// not force is an authored one and becomes a blank line (a paragraph, ready
// for display), forced wraps join with a space
static juce::String mergeComment ( const juce::String& firstValue, const juce::StringArray& contLines )
{
	constexpr auto	fieldIndent = 9;

	// The entry's own wrap column; STIL's house style fills to ~78
	auto	wrapWidth = 78;
	for ( const auto& l : contLines )
		wrapWidth = std::max ( wrapWidth, l.trimEnd ().length () );

	auto	result = firstValue;
	auto	prevTotal = fieldIndent + firstValue.trimEnd ().length ();
	auto	prevText = firstValue.trimEnd ();
	bool	blankBreak = false;

	for ( const auto& raw : contLines )
	{
		// A blank line in the source is an explicit paragraph break
		if ( raw.trim ().isEmpty () )
		{
			blankBreak = true;
			continue;
		}

		const auto	line = raw.substring ( fieldIndent );
		const auto	trimmed = line.trim ();

		auto isBreak = [ & ]
		{
			if ( startsLongQuote ( line ) )
				return true;

			if ( line.startsWithChar ( ' ' ) )		// authored extra indent
				return true;

			// a clearly unfilled line ends its paragraph
			const auto	prevLen = prevTotal - fieldIndent;
			if ( prevLen < 50 || ( prevText.endsWithChar ( '.' ) && prevLen < 60 ) )
				return true;

			// a wrap that was not forced (the next word would still have fit)
			// marks a paragraph, but only when the previous line closes a
			// sentence (a path reference counts) and the next opens one
			const auto	firstWord = trimmed.upToFirstOccurrenceOf ( " ", false, false );
			if ( prevTotal + 1 + firstWord.length () > wrapWidth )
				return false;

			const auto	nextStart = trimmed[ 0 ];

			return ( juce::String ( ".!?:;\")" ).containsChar ( prevText.getLastCharacter () ) || prevText.endsWith ( ".sid" ) )
				&& ( juce::CharacterFunctions::isUpperCase ( nextStart )
					|| juce::CharacterFunctions::isDigit ( nextStart )
					|| nextStart == '"' || nextStart == '(' );
		};

		result += juce::String ( blankBreak || isBreak () ? "\n\n" : " " ) + line;
		blankBreak = false;

		prevTotal = raw.trimEnd ().length ();
		prevText = line.trimEnd ();
	}

	return result;
}
//-----------------------------------------------------------------------------

void HVSC_database::loadSTIL ()
{
	const juce::ScopedLock	sl ( dbLock );

	stilDB.clear ();

	// Load file into separate lines
	auto loadEntries = [ this ] ( const juce::String& content )
	{
		if ( content.isEmpty () )
			return;

		const auto	fileStrings = juce::StringArray::fromLines ( content );

		// Parse STIL blocks
		std::string	tuneFilename;
		STIL_tune	tune;
		int			tuneNo = 0;

		auto storeTune = [ &tune, this ] ( const std::string& _tuneFilename )
		{
			if ( tune.empty () )
				return;

			for ( auto& tuneIV : tune )
				tuneIV.second.shrink_to_fit ();

			if ( auto it = stilDB.find ( _tuneFilename ); it != stilDB.end () )
			{
				auto&	dst = it->second;

				// Merge into existing entry: a field the entry already has is
				// an override and replaces it, new fields go in front
				for ( const auto& [ number, vec ] : tune )
				{
					auto&	dstVec = dst[ number ];

					for ( auto fieldIt = vec.rbegin (); fieldIt != vec.rend (); ++fieldIt )
					{
						auto	match = std::find_if ( dstVec.begin (), dstVec.end (), [ & ] ( const auto& field ) { return field.first == fieldIt->first; } );

						if ( match != dstVec.end () )
							match->second = fieldIt->second;
						else
							dstVec.insert ( dstVec.begin (), *fieldIt );
					}
				}
			}
			else
			{
				// New entry
				stilDB[ _tuneFilename ] = tune;
			}
			tune.clear ();
		};

		// Continuation lines buffer up per field; the merge runs when it closes
		std::pair<std::string, std::string>*	openField = nullptr;
		juce::StringArray						openLines;

		auto flushField = [ &openField, &openLines ] ()
		{
			if ( openField && ! openLines.isEmpty () )
			{
				if ( openField->first == "COMMENT" )
					openField->second = mergeComment ( openField->second, openLines ).toStdString ();
				else
				{
					// Non-comment fields merge plainly
					auto	value = juce::String ( openField->second );

					for ( const auto& raw : openLines )
					{
						if ( raw.isEmpty () )
							continue;

						const auto	stringToAdd = raw.substring ( 9 );
						const auto	prefix = juce::String ( startsLongQuote ( stringToAdd ) ? "\n" : " " );

						juce::String	extra;
						if ( prefix == " " )
							extra = ( stringToAdd.length () < 50 || ( stringToAdd.endsWithChar ( '.' ) && stringToAdd.length () < 60 ) ) ? "\n" : "";

						value += prefix + stringToAdd + extra;
					}

					openField->second = value.toStdString ();
				}
			}

			openField = nullptr;
			openLines.clear ();
		};

		for ( const auto& s : fileStrings )
		{
			// Comment
			if ( s.startsWithChar ( '#' ) )
				continue;

			// A blank line inside a wrapped field is an explicit paragraph break
			if ( s.isEmpty () )
			{
				if ( openField )
					openLines.add ( juce::String () );

				continue;
			}

			// New block
			if ( s.startsWithChar ( '/' ) )
			{
				flushField ();
				storeTune ( tuneFilename );

				tuneFilename = lime::str::toLower ( s.toStdString () );
				tuneNo = 0;
				continue;
			}

			// New tune
			if ( s.startsWithChar ( '(' ) )
			{
				flushField ();
				tuneNo = std::atoi ( s.substring ( 2 ).dropLastCharacters ( 1 ).toRawUTF8 () );
				continue;
			}

			// Multi-line field continuation
			if ( s.startsWith ( "         " ) )		// nine (9) spaces
			{
				if ( openField )
					openLines.add ( s );

				continue;
			}

			flushField ();

			auto&		tuneRef = tune[ tuneNo ];
			const auto	name = s.substring ( 0, 9 ).toUpperCase ();

			tuneRef.emplace_back ( std::pair { name.dropLastCharacters ( 2 ).trimStart ().toStdString (), s.substring ( 9 ).toStdString () } );
			openField = &tuneRef.back ();
		}

		// Store final block
		flushField ();
		storeTune ( tuneFilename );
	};

	loadEntries ( hvscsource::loadText ( "DOCUMENTS/STIL.txt" ) );
	if ( stilDB.empty () )
	{
		errorCode = "STIL missing";
		return;
	}

	// Load overrides
	loadEntries ( datasource::loadText ( "Databases/STIL-addendum.txt" ) );
}
//-----------------------------------------------------------------------------

void HVSC_database::loadBugs ()
{
	const juce::ScopedLock	sl ( dbLock );

	// Open file
	const auto	text = hvscsource::loadText ( "DOCUMENTS/BUGlist.txt" );
	if ( text.isEmpty () )
		return;

	// Load file into separate lines
	const auto	fileStrings = juce::StringArray::fromLines ( text );

	// Parse STIL blocks
	std::string	tuneFilename;
	STIL_tune	tune;

	auto mergeIntoTune = [ &tune, this ] ( const std::string& _tuneFilename )
	{
		if ( tune.empty () )
			return;

		if ( auto it = stilDB.find ( _tuneFilename ); it != stilDB.end () )
		{
			auto&	tuneRef = it->second[ 0 ];

			for ( const auto& pair : tune[ 0 ] )
				tuneRef.emplace_back ( pair );
		}
		else
		{
			stilDB[ _tuneFilename ] = tune;
		}

		tune.clear ();
	};

	for ( const auto& s : fileStrings )
	{
		// Comment or empty
		if ( s.startsWithChar ( '#' ) || s.isEmpty () )
			continue;

		// New block
		if ( s.startsWithChar ( '/' ) )
		{
			mergeIntoTune ( tuneFilename );

			tuneFilename = lime::str::toLower ( s.toStdString () );
			continue;
		}

		// Author
 		if ( s.startsWithChar ( '(' ) )
 		{
 			if ( ! tune[ 0 ].empty () )
 				tune[ 0 ].back ().second += "\n" + s.toStdString ();
 			continue;
 		}

		// Handle multi-line fields
		if ( s.startsWith ( "         " ) )		// nine (9) spaces
		{
			// Extend last std::string of the vector
			if ( ! tune[ 0 ].empty () )
				tune[ 0 ].back ().second += s.substring ( 8 ).toStdString ();
			continue;
		}

		const auto	name = s.substring ( 0, 9 ).toUpperCase ();
		if ( name == "    BUG: " )
			tune[ 0 ].emplace_back ( std::pair { "BUG", s.substring ( 9 ).toStdString () } );
	}

	// Store final block
	mergeIntoTune ( tuneFilename );
}
//-----------------------------------------------------------------------------
