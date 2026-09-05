#pragma once

#include <JuceHeader.h>

#include "App/InstallState.h"
#include "App/SharedProfiles.h"
#include "Database/Database.h"
#include "Database/HVSCDatabase.h"

class SIDPlayer;

//-----------------------------------------------------------------------------

// The application's content roots (HVSC collection, user folder; factory data
// lives behind datasource): what a valid install looks like, and the non-UI
// bootstrap steps that load databases, ROMs, profiles and the sidid config
// from them into the shared globals and the player engine. The UI wires the
// roots into its components and decides the orchestration order.

class AppRoots final
{
public:
	juce::File	hvsc;
	juce::File	user;

	[[ nodiscard ]] bool isHVSCValid () const;

	// Does this folder itself hold a complete collection?
	[[ nodiscard ]] static bool hasHVSCContent ( const juce::File& folder );

	// Maps a folder the user picked to the actual root: the folder itself when it already
	// holds a collection, its "C64Music" child when that does, otherwise where an install
	// would put one. The result is always the C64Music folder the archive unpacks
	[[ nodiscard ]] static juce::File resolveHVSCRoot ( const juce::File& chosen );

	// Attach the HVSC scanner to the hvsc root; returns the installed
	// version (also published to InstallState)
	int attachHVSCDatabase ();

	void loadROMs ( SIDPlayer& player );

	// Load ultraSID.db, apply tune overrides, publish versions to InstallState
	void loadDatabase ( SIDPlayer& player );

	// Parse sidid.cfg into a fresh shared player config, keeping the already
	// loaded profiles and overrides
	void loadSidIDConfig ( SIDPlayer& player );

	// Reload the three profile/override CSVs; returns true when tune
	// overrides were reloaded (callers refresh override-dependent views)
	bool loadProfilesAndOverrides ( SIDPlayer& player );

	// Set by the last loadProfilesAndOverrides (): names the first cell that would
	// not convert, empty when all three files were clean
	[[ nodiscard ]] const juce::String& getProfileError () const	{	return profileError;	}

private:
	juce::String	profileError;

	juce::SharedResourcePointer<HVSC_database>		hvscDatabase;
	juce::SharedResourcePointer<Database>			database;
	juce::SharedResourcePointer<InstallState>		installState;
	juce::SharedResourcePointer<SharedProfiles>		profiles;
};
//-----------------------------------------------------------------------------
