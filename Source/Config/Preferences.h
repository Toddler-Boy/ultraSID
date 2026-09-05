#pragma once

#include <JuceHeader.h>

#include <algorithm>

#include "ultra-shared/Config/SharedPreferences.h"
#include "ultra-shared/Config/YamlFile.h"

//-----------------------------------------------------------------------------

class Preferences final : public YamlFile
{
public:
	Preferences () : YamlFile ( getDefaultValues () )
	{
	}

	void setRoot ( const juce::File& _root )
	{
		if ( _root == juce::File () )
			return;

		Z_DLOG ( "loading preferences" );
		load ( _root.getChildFile ( "preferences.yml" ) );
	}

	struct range { int min = 0, max = 100; };

	// Valid ranges for the free-form number settings, shared by the UI editor
	// (rejects nonsense input) and the consumers (clamp hand-edited yml values)
	[[ nodiscard ]] static range getRange ( const juce::String& key )
	{
		if ( key == "fx/transition-time" )	return { 0, 10 };	// seconds per mode hop, 0 = instant

		if ( key == "songs/unknown" )	return { 1, 20 };	// minutes
		if ( key == "songs/minimum" )	return { 0, 600 };	// seconds
		if ( key == "songs/max-loops" )	return { 0, 99 };
		if ( key == "songs/fade-out" )	return { 0, 60 };	// seconds

		return {};
	}

	[[ nodiscard ]] int getClamped ( const juce::String& key )
	{
		const auto	r = getRange ( key );

		return std::clamp ( get<int> ( key ), r.min, r.max );
	}

private:
	[[ nodiscard ]] static std::vector<YamlFile::value> getDefaultValues ()
	{
		std::vector<YamlFile::value>	out
		{
			{ "ui",			"keep-screen-awake",	false },
			{ "ui",			"theme",				"$DATA$/default" },
			{ "ui",			"gamma",				1.0 },
			{ "ui",			"brightness",			1.0 },
			{ "ui",			"contrast",				1.0 },
			{ "ui",			"saturation",			1.0 },

			{ "stil",		"show-tunes-only",		true },
			{ "stil",		"show-information",		true },
			{ "stil",		"viz-always",			false },

			{ "player",		"volume",				100 },
			{ "player",		"mute",					false },
			{ "player",		"quality",				"MAGIC" },
			{ "player",		"normalize",			true },
			{ "player",		"show-length",			false },
			{ "player",		"boot-screen",			"Basic C64" },
			{ "player",		"player-screen",		"Random" },

			{ "songs",		"unknown",				5 },
			{ "songs",		"minimum",				60 },
			{ "songs",		"max-loops",			5 },
			{ "songs",		"fade-out",				10 },

			{ "emulation",	"dac-leakage",			false },

			{ "fx",			"stereo-processing",	true },
			{ "fx",			"transition-time",		0.3f },

			{ "eq",			"low",					0.0f },
			{ "eq",			"mid",					0.0f },
			{ "eq",			"high",					0.0f },

			{ "export",		"name-template",		"{A} - {T} {N}" },
			{ "export",		"format",				"WAV" },
			{ "export",		"normalize",			false },
		};

		// The CRT-emulation block (overlay, tv, crt, webcam) is shared with
		// ultraView, keys and values identical by construction
		const auto	shared = sharedpreferences::getDefaultValues ();
		out.insert ( out.end (), shared.begin (), shared.end () );

		return out;
	}
};
//-----------------------------------------------------------------------------
