#include "AppRoots.h"

#include "ultra-shared/Config/DataSource.h"

#include "Audio/SIDPlayer.h"
#include "Config/FilePaths.h"
#include "Config/HVSCSource.h"

//-----------------------------------------------------------------------------

bool AppRoots::hasHVSCContent ( const juce::File& folder )
{
	// Not exhaustive, the collection is far too large and changes constantly. HVSC.txt is
	// in here because the installed version is read from it, so a folder that validates
	// can always be version-checked and offered an update
	static const juce::StringArray	arr = {
		"DEMOS/",
		"DOCUMENTS/",
		"GAMES/",
		"MUSICIANS/",
		"DOCUMENTS/Songlengths.md5",
		"DOCUMENTS/STIL.txt",
		"DOCUMENTS/BUGlist.txt",
		"DOCUMENTS/HVSC.txt",

		// The last letter folder, so a truncated unpack is unlikely to have reached it.
		// A structural folder, not a tune, so an update's MOVE/DELETE cannot remove it
		"MUSICIANS/Z/",

		// What the reference HVSC updater sanity-checks for (Update.cpp, LONG_HUBBARD_ROB_DIR).
		// A directory rather than tunes, present in every release from 46 on
		"MUSICIANS/H/Hubbard_Rob/",
	};

	// Also true for a zip archive holding the collection
	return hvscsource::allPathsValid ( folder, arr );
}
//-----------------------------------------------------------------------------

juce::File AppRoots::resolveHVSCRoot ( const juce::File& chosen )
{
	if ( hasHVSCContent ( chosen ) )
		return chosen;

	// A zip is the root itself: an existing archive or the file an install
	// will create
	if ( hvscsource::isZipArchive ( chosen ) || ( ! chosen.exists () && chosen.hasFileExtension ( "zip" ) ) )
		return chosen;

	// An archive already in the picked folder wins
	for ( const auto* name : { "HVSC.zip", "C64Music.zip" } )
		if ( const auto zipped = chosen.getChildFile ( name ); hasHVSCContent ( zipped ) )
			return zipped;

	// The user may have picked the folder the collection sits in rather than the
	// collection itself
	if ( const auto nested = chosen.getChildFile ( "C64Music" ); hasHVSCContent ( nested ) )
		return nested;

	// Fresh install target: HVSC.zip, or the loose layout a folder already
	// named C64Music implies
	return chosen.getFileName ().equalsIgnoreCase ( "C64Music" ) ? chosen : chosen.getChildFile ( "HVSC.zip" );
}
//-----------------------------------------------------------------------------

bool AppRoots::isHVSCValid () const
{
	// Does folder even exist?
	if ( ! hvsc.isDirectory () && ! hvscsource::isZipArchive ( hvsc ) )
	{
		Z_ERR ( "HVSC root not set or invalid path: " << hvsc.getFullPathName () );
		return false;
	}

	// Is it complete?
	const auto	hvscValid = hasHVSCContent ( hvsc );
	if ( ! hvscValid )
		Z_ERR ( "HVSC incomplete (missing folders or files) " << hvsc.getFullPathName () );

	return hvscValid;
}
//-----------------------------------------------------------------------------

int AppRoots::attachHVSCDatabase ()
{
	hvscDatabase->attach ();
	installState->hvsc.versionInstalled = hvscDatabase->getHVSCVersion ();

	return installState->hvsc.versionInstalled;
}
//-----------------------------------------------------------------------------

void AppRoots::loadROMs ( SIDPlayer& player )
{
	profiles->loadRoms ();

	const auto	[ kernal, basic, character ]  = profiles->getRoms ();
	player.setRoms ( kernal, basic, character );
}
//-----------------------------------------------------------------------------

void AppRoots::loadDatabase ( SIDPlayer& player )
{
	installState->database.versionInstalled = database->load ( datasource::loadData ( "ultraSID.db" ) );

	if ( installState->database.versionInstalled <= 0 )
		Z_ERR ( "The tune database (ultraSID.db) is unreadable, the browser stays empty" );

	database->applyOverrides ( player.getAllTuneOverrides () );
}
//-----------------------------------------------------------------------------

void AppRoots::loadSidIDConfig ( SIDPlayer& player )
{
	// Keep whatever profiles and overrides are already loaded
	auto	newConfig = std::make_shared<libsidplayEZ::SharedPlayerConfig> ();
	if ( auto current = profiles->getPlayerConfig () )
		*newConfig = *current;

	newConfig->loadSidIDConfigText ( datasource::loadText ( "sidid.cfg" ).toStdString () );

	profiles->setPlayerConfig ( newConfig );
	player.setSharedConfig ( newConfig );
}
//-----------------------------------------------------------------------------

bool AppRoots::loadProfilesAndOverrides ( SIDPlayer& player )
{
	//
	// Build a fresh shared config, starting from the current one so the already
	// parsed sidid signatures survive, and reload the three CSVs on it. Players
	// keep the config they were given until they get the new one
	//
	auto	newConfig = std::make_shared<libsidplayEZ::SharedPlayerConfig> ();
	if ( auto current = profiles->getPlayerConfig () )
		*newConfig = *current;

	// A hand-edited csv keeps its defaults for any cell that will not convert. The
	// first complaint is kept for the UI, which is how the user finds out at all
	profileError.clear ();

	auto reportBadCell = [ this ] ( const juce::String& name, const std::string& err )
	{
		if ( err.empty () )
			return;

		const auto	text = name + ": " + juce::String ( err );

		Z_ERR ( text );

		if ( profileError.isEmpty () )
			profileError = text;
	};

	if ( auto text = datasource::loadText ( "Databases/chip-profiles.csv" ); text.isNotEmpty () )
		reportBadCell ( "chip-profiles.csv", newConfig->loadChipProfiles ( text.toStdString () ) );

	// An optional user overlay merges over the factory profiles: rows with a
	// known name replace them whole, new names add profiles
	if ( auto f = user.getChildFile ( "chip-profiles.csv" ); f.existsAsFile () )
		reportBadCell ( f.getFileName (), newConfig->loadChipProfiles ( f.loadFileAsString ().toStdString (), true ) );

	if ( auto text = datasource::loadText ( "Databases/audio-profiles.csv" ); text.isNotEmpty () )
		reportBadCell ( "audio-profiles.csv", newConfig->loadAudioProfiles ( text.toStdString () ) );

	if ( auto text = datasource::loadText ( "Databases/digi-players.csv" ); text.isNotEmpty () )
		reportBadCell ( "digi-players.csv", newConfig->loadDigiPlayers ( text.toStdString () ) );

	if ( auto text = datasource::loadText ( "Databases/digi-tunes.csv" ); text.isNotEmpty () )
		reportBadCell ( "digi-tunes.csv", newConfig->loadDigiTunes ( text.toStdString () ) );

	auto	overridesLoaded = false;
	if ( auto text = datasource::loadText ( "Databases/tune-overrides.csv" ); text.isNotEmpty () )
	{
		reportBadCell ( "tune-overrides.csv", newConfig->loadTuneOverrides ( text.toStdString () ) );
		overridesLoaded = true;
	}

	profiles->setPlayerConfig ( newConfig );
	player.setSharedConfig ( newConfig );

	if ( overridesLoaded )
		database->applyOverrides ( player.getAllTuneOverrides () );

	return overridesLoaded;
}
//-----------------------------------------------------------------------------
