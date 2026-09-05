#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Config/YamlFile.h"

//-----------------------------------------------------------------------------

class Settings final : public YamlFile
{
public:
	Settings () : YamlFile ( getDefaultValues () )
	{
		auto	file = juce::File::getSpecialLocation ( juce::File::SpecialLocationType::userApplicationDataDirectory )
						.getChildFile ( ProjectInfo::projectName )
						.getChildFile ( "settings.yml" );

		load ( file );
	}

	// The folder name the user data lives in by default, "ultraSID user-data"
	[[ nodiscard ]] static juce::String userDataFolderName ()
	{
		return juce::String ( ProjectInfo::projectName ) + " user-data";
	}

private:
	[[ nodiscard ]] static std::vector<YamlFile::value> getDefaultValues ()
	{
		return {
			{ "output",		"device",			"System default" },

			{ "ui",			"window-position",	"" },
			{ "ui",			"page",				"search" },
			{ "ui",			"crt-image",		0 },

			{ "ui",			"search-str",		"" },
			{ "ui",			"search-selected",	0 },
			{ "ui",			"search-pos",		0.0 },
			{ "ui",			"search-col",		4 },
			{ "ui",			"search-forwards",	true },

			{ "ui",			"playlist",				"" },
			{ "ui",			"playlist-selected",	0 },
			{ "ui",			"playlist-pos",			0.0 },

			// Set when a full install starts, cleared when it completes; found
			// set at startup = the install was killed, the tree is suspect
			{ "hvsc",		"install-in-progress",	false },

			{ "update",		"last-check",			"" },
			{ "update",		"last-known-version",	"" },

			// Bare folder: resolveHVSCRoot finds a collection inside it or
			// targets its HVSC.zip
			{ "paths",		"hvsc",			juce::SystemStats::getEnvironmentVariable ( "HVSC_BASE", juce::File::getSpecialLocation ( juce::File::SpecialLocationType::userDocumentsDirectory ).getFullPathName () ).toStdString () },
			{ "paths",		"user",			juce::File::getSpecialLocation ( juce::File::SpecialLocationType::userDocumentsDirectory ).getChildFile ( userDataFolderName () ).getFullPathName ().toStdString () },
			{ "paths",		"export",		juce::File::getSpecialLocation ( juce::File::SpecialLocationType::userDocumentsDirectory ).getChildFile ( juce::String ( ProjectInfo::projectName ) + " exports" ).getFullPathName ().toStdString () },
		};
	}
};
//-----------------------------------------------------------------------------
