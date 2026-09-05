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

// The LUT key for an art filename ("A/B_01.png" -> "a/b")
static std::string artKey ( const std::string& filename )
{
	return lime::str::toLower ( filename.substr ( 0, filename.find_last_of ( '_' ) ) );
}
//-----------------------------------------------------------------------------

void ScreenshotLookup::reload ()
{
	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	tuneFileToArtFiles.clear ();

	// Scan for art files (C64 titles screens and game play screen-shots)
	for ( const auto& f : datasource::listFiles ( "Screenshots/", true, "*.png" ) )
	{
		const auto	filename = f.toStdString ();

		// Add to LUT
		tuneFileToArtFiles[ artKey ( filename ) ].emplace_back ( filename );
	}

	// Sort filenames
	for ( auto& [ _, files ] : tuneFileToArtFiles )
		std::ranges::sort ( files );
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
		if ( imageutils::hintFromFilename ( scr ).isGameScreen )
			return index;

		++index;
	}

	return 0;
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

	// Add to LUT
	if ( auto it = tuneFileToArtFiles.find ( tunename ); it != tuneFileToArtFiles.end () )
	{
		if ( auto itFile = std::ranges::find_if ( it->second, findWithHint ); itFile == it->second.end () )
		{
			it->second.emplace_back ( filename );
			std::ranges::sort ( it->second );
		}
		else
		{
			// Developer curation normalizes the incoming name onto the known one
			datasource::getDevFile ( "Screenshots/" + filename ).moveFileTo ( datasource::getDevFile ( "Screenshots/" + *itFile ) );
		}
	}
	else
	{
		tuneFileToArtFiles[ tunename ] = { filename };
	}
}
//-----------------------------------------------------------------------------

void ScreenshotLookup::removeScreenshot ( const std::string& filename )
{
	const auto	tunename = artKey ( filename );

	const juce::CriticalSection::ScopedLockType	csLock ( lutCs );

	// Remove from LUT
	if ( auto it = tuneFileToArtFiles.find ( tunename ); it != tuneFileToArtFiles.end () )
		if ( auto it2 = std::ranges::find ( it->second, filename ); it2 != it->second.end () )
		{
			it->second.erase ( it2 );

			if ( it->second.empty () )
				tuneFileToArtFiles.erase ( it );
		}
}
//-----------------------------------------------------------------------------
