#include <JuceHeader.h>

#include "FilePaths.h"

#include "ultra-shared/Config/DataSource.h"

#include "Config/HVSCSource.h"
#include "Config/Settings.h"

//-----------------------------------------------------------------------------

bool filepaths::allPathsValid ( const juce::StringArray& arr, const juce::File& root )
{
	for ( const auto& f : arr )
	{
		if ( f.endsWithChar ( '/' ) )
		{
			if ( ! root.getChildFile ( f ).isDirectory () )
				return false;
		}
		else
		{
			if ( ! root.getChildFile ( f ).existsAsFile () )
				return false;
		}
	}

	return true;
}
//-----------------------------------------------------------------------------

static juce::File getUserPath ( const juce::String& folder )
{
	const juce::SharedResourcePointer<Settings>	settings;

	auto	path = settings->get<juce::String> ( "paths/user" );
	if ( path.isEmpty () )
		return {};

	auto	subFolder = juce::File ( path ).getChildFile ( folder );
	subFolder.createDirectory ();

	return subFolder;
}
//-----------------------------------------------------------------------------

// The export list and the loudness cache live with the rest of the user
// data, visible and part of its backups
static juce::File getUserDataFile ( const juce::String& name )
{
	const juce::SharedResourcePointer<Settings>	settings;

	const auto	path = settings->get<juce::String> ( "paths/user" );
	if ( path.isEmpty () )
		return {};

	return juce::File ( path ).getChildFile ( name );
}
//-----------------------------------------------------------------------------

juce::File filepaths::getExportListPath ()
{
	return getUserDataFile ( "exports.csv" );
}
//-----------------------------------------------------------------------------

juce::File filepaths::getUserLoudnessPath ()
{
	return getUserDataFile ( "SID_LUFS.txt" );
}
//-----------------------------------------------------------------------------

juce::File filepaths::getPlaylistsPath ()
{
	return getUserPath ( "Playlists" );
}
//-----------------------------------------------------------------------------

juce::File filepaths::getUserTunesPath ()
{
	return getUserPath ( "Tunes" );
}
//-----------------------------------------------------------------------------

juce::File filepaths::getUserOverlaysPath ()
{
	return getUserPath ( "Overlays" );
}
//-----------------------------------------------------------------------------

juce::File filepaths::getUserCRTMasksPath ()
{
	return getUserPath ( "CRT Masks" );
}
//-----------------------------------------------------------------------------

juce::File filepaths::getUserCRTPresetsPath ()
{
	return getUserPath ( "CRT Presets" );
}
//-----------------------------------------------------------------------------

juce::String filepaths::markerFor ( const root which )
{
	// The markers are literals, so data () is NUL-terminated
	switch ( which )
	{
		case root::hvsc:	return hvscMarker.data ();
		case root::user:	return userMarker.data ();

		default:			return dataMarker.data ();
	}
}
//-----------------------------------------------------------------------------

filepaths::TuneSource filepaths::resolveTune ( const juce::String& markedPath )
{
	auto	which = root::data;
	auto	child = markedPath;

	for ( const auto candidate : { root::data, root::hvsc, root::user } )
	{
		if ( const auto marker = markerFor ( candidate ); markedPath.startsWith ( marker ) )
		{
			which = candidate;
			child = markedPath.substring ( marker.length () );
			break;
		}
	}

	// A leading separator would make a child path absolute
	if ( child.startsWithChar ( '/' ) )
		child = child.substring ( 1 );

	if ( child.isEmpty () )
		return {};

	switch ( which )
	{
		case root::hvsc:
		{
			if ( hvscsource::isZipMode () )
			{
				if ( hvscsource::exists ( child ) )
					return { {}, {}, child };
			}
			else
			{
				const juce::SharedResourcePointer<Settings>	settings;

				if ( const auto hvscRoot = juce::File ( settings->get<juce::String> ( "paths/hvsc" ) ); hvscRoot != juce::File () )
					if ( auto f = hvscRoot.getChildFile ( child ); f.existsAsFile () )
						return { f, {} };
			}

			// Tunes the HVSC doesn't include yet ship with ultraSID in a folder
			// mirroring the HVSC layout; a file in the real collection always
			// shadows the mirror
			if ( const auto mirrored = "Exotic tunes/" + child; datasource::exists ( mirrored ) )
				return { {}, mirrored };

			return {};
		}

		case root::user:
		{
			if ( auto f = getUserTunesPath ().getChildFile ( child ); f.existsAsFile () )
				return { f, {} };

			return {};
		}

		default:
		{
			if ( datasource::exists ( child ) )
				return { {}, child };

			return {};
		}
	}
}
//-----------------------------------------------------------------------------

juce::String filepaths::TuneSource::toLoadable () const
{
	if ( file != juce::File () )
		return file.getFullPathName ();

	if ( hvscPath.isNotEmpty () )
		return juce::String ( hvscMarker.data () ) + "/" + hvscPath;

	return dataPath;
}
//-----------------------------------------------------------------------------

std::string_view filepaths::stripLocationMarker ( std::string_view tuneKey )
{
	if ( tuneKey.starts_with ( '$' ) )
		if ( const auto end = tuneKey.find ( '$', 1 ); end != std::string_view::npos )
			return tuneKey.substr ( end + 1 );

	return tuneKey;
}
//-----------------------------------------------------------------------------

juce::String filepaths::stripLocationMarker ( const std::string& tuneKey )
{
	const auto	plain = stripLocationMarker ( std::string_view ( tuneKey ) );

	return juce::String ( plain.data (), plain.size () );
}
//-----------------------------------------------------------------------------
