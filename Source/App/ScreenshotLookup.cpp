#include "ScreenshotLookup.h"

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/Helpers/ImageUtils.h"

#include "Config/FilePaths.h"


//-----------------------------------------------------------------------------

// The LUT key for a tune key ("$HVSC$/A/B.sid" -> "a/b"); empty when the tune
// is not from the HVSC, only HVSC tunes have screenshots
static std::string screenshotKey ( const std::string& tunename )
{
	if ( ! tunename.starts_with ( filepaths::hvscMarker ) )
		return {};

	// Marker and slash off, extension off, folded like the LUT keys
	auto	name = std::string ( filepaths::stripLocationMarker ( std::string_view ( tunename ) ) );
	if ( name.empty () || name.front () != '/' )
		return {};

	name.erase ( 0, 1 );

	return lime::str::toLower ( name.substr ( 0, name.find_last_of ( '.' ) ) );
}
//-----------------------------------------------------------------------------

// The LUT key for an art filename ("A/B_01#5.png" -> "a/b"); a name without
// the _NN number is a group of its own, hints aside
static std::string artKey ( const std::string& filename )
{
	const auto	base = imageutils::hintFromFilename ( filename ).name.toStdString ();

	return lime::str::toLower ( base.substr ( 0, base.find_last_of ( '_' ) ) );
}
//-----------------------------------------------------------------------------

// The name without its hints ("a/b_01#5.png" -> "a/b_01.png"), lowercase:
// what decides whether a user file replaces a factory one
static std::string stemKey ( const std::string& filename )
{
	const auto	hint = imageutils::hintFromFilename ( filename );

	return lime::str::toLower ( ( hint.name + hint.extension ).toStdString () );
}
//-----------------------------------------------------------------------------

// Case-insensitive, the exact spelling only breaks ties
static void sortNatural ( std::vector<std::string>& v )
{
	std::ranges::sort ( v, [] ( const std::string& a, const std::string& b )
	{
		const auto	byName = lime::str::naturalCompare ( std::string_view ( lime::str::toLower ( a ) ), std::string_view ( lime::str::toLower ( b ) ) );
		return byName != 0 ? byName < 0 : a < b;
	} );
}
//-----------------------------------------------------------------------------

void ScreenshotLookup::reload ()
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	names.clear ();
	userNames.clear ();

	userRoot = filepaths::getUserScreenshotsPath ();

	// Factory first, then the user files replace or add; hints do not make a
	// name distinct
	std::unordered_map<std::string, std::string>	byStem;

	for ( const auto& f : datasource::listFiles ( "Screenshots/", true, "*.png" ) )
	{
		const auto	filename = f.toStdString ();
		byStem[ stemKey ( filename ) ] = filename;
	}

	if ( userRoot.isDirectory () )
	{
		for ( const auto& f : userRoot.findChildFiles ( juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.png" ) )
		{
			const auto	filename = f.getRelativePathFrom ( userRoot ).replaceCharacter ( '\\', '/' ).toStdString ();

			byStem[ stemKey ( filename ) ] = filename;
			userNames.insert ( filename );
		}
	}

	for ( const auto& [ _, filename ] : byStem )
		names.insert ( filename );

	rebuildIndex ();
}
//-----------------------------------------------------------------------------

void ScreenshotLookup::rebuildIndex ()
{
	tuneFileToArtFiles.clear ();
	tree.clear ();

	for ( const auto& filename : names )
	{
		tuneFileToArtFiles[ artKey ( filename ) ].emplace_back ( filename );

		// Every folder on the way down gets the child, once
		auto	pos = filename.find_last_of ( '/' );
		const auto	folder = pos == std::string::npos ? std::string () : filename.substr ( 0, pos );

		tree[ lime::str::toLower ( folder ) ].files.emplace_back ( filename );

		for ( auto sub = folder; ! sub.empty (); )
		{
			pos = sub.find_last_of ( '/' );
			const auto	parent = pos == std::string::npos ? std::string () : sub.substr ( 0, pos );

			auto&	folders = tree[ lime::str::toLower ( parent ) ].folders;
			if ( ! std::ranges::contains ( folders, sub ) )
				folders.emplace_back ( sub );

			sub = parent;
		}
	}

	for ( auto& [ _, files ] : tuneFileToArtFiles )
		std::ranges::sort ( files );

	for ( auto& [ _, l ] : tree )
	{
		sortNatural ( l.folders );
		sortNatural ( l.files );
	}
}
//-----------------------------------------------------------------------------

std::vector<std::string> ScreenshotLookup::getScreenshots ( const std::string& tunename ) const
{
	const auto	key = screenshotKey ( tunename );
	if ( key.empty () )
		return {};

	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	auto	scrSht = tuneFileToArtFiles.find ( key );
	if ( scrSht == tuneFileToArtFiles.end () )
		return {};

	return scrSht->second;
}
//-----------------------------------------------------------------------------

std::string ScreenshotLookup::getDefaultScreenshot ( const std::string& tunename ) const
{
	const auto	shots = getScreenshots ( tunename );
	if ( shots.empty () )
		return {};

	return shots[ getDefaultScreenshotIndex ( shots ) ];
}
//-----------------------------------------------------------------------------

int ScreenshotLookup::getDefaultScreenshotIndex ( const std::vector<std::string>& screenshots )
{
	for ( auto index = 0; const auto& scr : screenshots )
	{
		if ( imageutils::hintFromFilename ( scr ).loadingScreen )
			return index;

		++index;
	}

	return 0;
}
//-----------------------------------------------------------------------------

std::vector<std::string> ScreenshotLookup::getSiblings ( const std::string& artName ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	if ( const auto it = tuneFileToArtFiles.find ( artKey ( artName ) ); it != tuneFileToArtFiles.end () )
		return it->second;

	return { artName };
}
//-----------------------------------------------------------------------------

int ScreenshotLookup::getLastNumber ( const std::string& tunename ) const
{
	auto	last = 0;

	for ( const auto& scr : getScreenshots ( tunename ) )
	{
		const auto	stem = imageutils::hintFromFilename ( scr ).name;
		last = std::max ( last, stem.fromLastOccurrenceOf ( "_", false, false ).getIntValue () );
	}

	return last;
}
//-----------------------------------------------------------------------------

bool ScreenshotLookup::exists ( const std::string& artName ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	return names.contains ( artName );
}
//-----------------------------------------------------------------------------

bool ScreenshotLookup::isUserFile ( const std::string& artName ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	return userNames.contains ( artName );
}
//-----------------------------------------------------------------------------

std::string ScreenshotLookup::currentName ( const std::string& artName ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	if ( names.contains ( artName ) )
		return artName;

	const auto	stem = stemKey ( artName );

	if ( const auto it = tuneFileToArtFiles.find ( artKey ( artName ) ); it != tuneFileToArtFiles.end () )
		for ( const auto& sibling : it->second )
			if ( stemKey ( sibling ) == stem )
				return sibling;

	return {};
}
//-----------------------------------------------------------------------------

juce::MemoryBlock ScreenshotLookup::loadData ( const std::string& artName ) const
{
	if ( isUserFile ( artName ) )
	{
		juce::MemoryBlock	mb;
		getUserFile ( artName ).loadFileAsData ( mb );
		return mb;
	}

	return datasource::loadData ( "Screenshots/" + artName );
}
//-----------------------------------------------------------------------------

juce::File ScreenshotLookup::getUserFile ( const std::string& artName ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	if ( userRoot == juce::File () )
		return {};

	return userRoot.getChildFile ( juce::String ( artName ) );
}
//-----------------------------------------------------------------------------

ScreenshotLookup::listing ScreenshotLookup::list ( const std::string& folder ) const
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	if ( const auto it = tree.find ( lime::str::toLower ( folder ) ); it != tree.end () )
		return it->second;

	return {};
}
//-----------------------------------------------------------------------------

ScreenshotLookup::listing ScreenshotLookup::search ( const std::string& folder, const juce::String& query ) const
{
	std::vector<std::string>	words;
	for ( const auto& w : juce::StringArray::fromTokens ( query.toLowerCase (), true ) )
		if ( w.isNotEmpty () )
			words.emplace_back ( w.toStdString () );

	const auto	prefix = folder.empty () ? std::string () : lime::str::toLower ( folder ) + "/";

	listing	result;

	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	for ( const auto& name : names )
	{
		auto	path = lime::str::toLower ( name );
		if ( ! path.starts_with ( prefix ) )
			continue;

		path.erase ( 0, prefix.size () );
		path.erase ( std::min ( path.find_last_of ( '.' ), path.size () ) );

		if ( const auto hash = path.find_last_of ( '#' ); hash != std::string::npos && hash > path.find_last_of ( '/' ) + 1 )
			path.erase ( hash );

		if ( std::ranges::all_of ( words, [ &path ] ( const std::string& w ) { return path.find ( w ) != std::string::npos; } ) )
			result.files.emplace_back ( name );
	}

	sortNatural ( result.files );

	return result;
}
//-----------------------------------------------------------------------------

void ScreenshotLookup::addScreenshot ( const std::string& filename )
{
	const auto	tunename = artKey ( filename );

	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	auto findWithHint = [ &filename ] ( const std::string& str ) -> bool
	{
		if ( filename == str )
			return true;

		const auto	hint = imageutils::hintFromFilename ( str );
		return filename == hint.name + hint.extension;
	};

	if ( auto it = tuneFileToArtFiles.find ( tunename ); it != tuneFileToArtFiles.end () )
	{
		if ( auto itFile = std::ranges::find_if ( it->second, findWithHint ); itFile != it->second.end () )
		{
			// Developer curation normalizes the incoming name onto the known one
			datasource::getDevFile ( "Screenshots/" + filename ).moveFileTo ( datasource::getDevFile ( "Screenshots/" + *itFile ) );
			return;
		}
	}

	names.insert ( filename );
	rebuildIndex ();
}
//-----------------------------------------------------------------------------

void ScreenshotLookup::removeScreenshot ( const std::string& filename )
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	if ( names.erase ( filename ) )
		rebuildIndex ();
}
//-----------------------------------------------------------------------------
