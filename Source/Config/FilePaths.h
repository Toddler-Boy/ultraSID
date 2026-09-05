#pragma once

#include <JuceHeader.h>

#include <string_view>

//-----------------------------------------------------------------------------

namespace filepaths
{
	// A stored tune key names the root it lives under, e.g. "$HVSC$/DEMOS/Tune.sid"
	enum class root { data, hvsc, user };

	// The location markers, single source of the "$XXX$" prefixes
	inline constexpr std::string_view dataMarker = "$DATA$";
	inline constexpr std::string_view hvscMarker = "$HVSC$";
	inline constexpr std::string_view userMarker = "$USER$";

	[[ nodiscard ]] juce::String markerFor ( const root which );

	// Where a tune key's bytes live: a real file (loose HVSC collection, user
	// tunes), a collection-relative path inside the HVSC zip, or a factory-data
	// path read through datasource (the Exotic-tunes mirror)
	struct TuneSource
	{
		juce::File		file;
		juce::String	dataPath;
		juce::String	hvscPath;

		[[ nodiscard ]] bool isValid () const	{	return file != juce::File () || dataPath.isNotEmpty () || hvscPath.isNotEmpty ();	}

		// What SIDPlayer::loadTune consumes: an absolute file path, a
		// "$HVSC$/..." key for zip-backed collection tunes, or a data path
		[[ nodiscard ]] juce::String toLoadable () const;
	};

	[[ nodiscard ]] TuneSource resolveTune ( const juce::String& markedPath );

	// Strips the location marker off a tune key, e.g. "$HVSC$/DEMOS/Tune.sid" -> "/DEMOS/Tune.sid"
	[[ nodiscard ]] std::string_view stripLocationMarker ( std::string_view tuneKey );
	[[ nodiscard ]] juce::String stripLocationMarker ( const std::string& tuneKey );

	// Every entry exists under root: trailing '/' = directory, otherwise file
	[[ nodiscard ]] bool allPathsValid ( const juce::StringArray& arr, const juce::File& root );

	[[ nodiscard ]] juce::File getPlaylistsPath ();
	[[ nodiscard ]] juce::File getExportListPath ();
	[[ nodiscard ]] juce::File getUserLoudnessPath ();
	[[ nodiscard ]] juce::File getUserTunesPath ();

	// User CRT content: real folders merged over the pak-backed factory set by
	// the lime content loader (user overlays shadow whole folders, user masks
	// shadow single files); presets are plain files read directly
	[[ nodiscard ]] juce::File getUserOverlaysPath ();
	[[ nodiscard ]] juce::File getUserCRTMasksPath ();
	[[ nodiscard ]] juce::File getUserCRTPresetsPath ();
}
//-----------------------------------------------------------------------------
