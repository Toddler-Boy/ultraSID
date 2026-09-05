#include "ultra-shared/Config/DataSource.h"

#include "Config/TunePatches.h"

#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

namespace
{
	// The watched factory root holding this file: the app Data folder or the
	// ultra-shared fallback root; invalid in pak mode, where nothing is watched
	[[ nodiscard ]] juce::File containingDataRoot ( const juce::File& file )
	{
		if ( datasource::isPak () )
			return {};

		if ( const auto root = datasource::getDevFile (); file.isAChildOf ( root ) )
			return root;

		if ( const auto shared = datasource::getSharedDevRoot (); file.isAChildOf ( shared ) )
			return shared;

		return {};
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::fileChanged ( const juce::File& file, gin::FileSystemWatcher::FileSystemEvent event )
{
	//	Z_INFO ( file.getFullPathName () );
	//	Z_INFO ( juce::String ( int ( event ) ) );

	if ( file.isDirectory () )
		return;

	const auto	fileChanged = event == gin::FileSystemWatcher::fileUpdated || event == gin::FileSystemWatcher::fileRenamedNewName;

	//
	// Change happened inside the data folder (only watched in developer mode,
	// where the factory data is the naked repo folder)
	//
	if ( const auto dataRoot = containingDataRoot ( file ); dataRoot != juce::File () )
	{
		auto	parent = file.getRelativePathFrom ( dataRoot ).replaceCharacter ( '\\', '/' );

		// Screenshots
		if ( parent.startsWithIgnoreCase ( "Screenshots/" ) )
		{
			if ( ! file.hasFileExtension ( ".png" ) )
				return;

			auto	filename = parent.fromFirstOccurrenceOf ( "/", false, false ).toStdString ();
			auto	updateCRT = true;

			if ( event == gin::FileSystemWatcher::fileCreated || event == gin::FileSystemWatcher::fileRenamedNewName )
			{
				screenshots->addScreenshot ( filename );
			}
			else if ( event == gin::FileSystemWatcher::fileDeleted || event == gin::FileSystemWatcher::fileRenamedOldName )
			{
				screenshots->removeScreenshot ( filename );
				filename = "";

				if ( event == gin::FileSystemWatcher::fileRenamedOldName )
					updateCRT = false;
			}
			else if ( event == gin::FileSystemWatcher::fileUpdated )
			{
				screenshots->removeScreenshot ( filename );
				screenshots->addScreenshot ( filename );
			}

			// In case the artwork is currently visible in the browser
			mainScreen.pages.repaint ();

			// Update footer thumbnail
			thumbnailCache->removeCacheEntry ( lastFilename );
			updateFooterThumbnail ( lastFilename );

			// Set CRT page to new artwork
			if ( updateCRT )
				mainScreen.pages.loadGameArtwork ( lastFilename, filename );

			return;
		}

		// Theme
		if ( parent.startsWithIgnoreCase ( "UI/themes/" ) )
		{
			if ( ! fileChanged )
				return;

			// Reload theme
			if ( file == theme->resolve ( preferences->get<juce::String> ( "ui/theme" ) ) )
				loadTheme ();

			return;
		}

		// Overlays
		if ( parent.startsWithIgnoreCase ( "Overlays/" ) )
		{
			if ( ! fileChanged )
				return;

			// Reload monitor profile
			if ( parent.endsWithIgnoreCase ( ".yml" ) )
				mainScreen.pages.reloadOverlayProfile ();

			return;
		}

		// UI strings
		if ( parent.startsWithIgnoreCase ( "UI/strings/" ) )
		{
			if ( ! fileChanged )
				return;

			strings->load ();
			repaint ();
			mainScreen.footer.repaintQualitySelector ();
		}

		// UI icons
		if ( parent.equalsIgnoreCase ( "UI/icons.yml" ) )
		{
			if ( ! fileChanged )
				return;

			icons->load ();
			repaint ();
			mainScreen.footer.repaintQualitySelector ();
		}

		// Hand-drawn player screens and their wash sidecars
		if ( parent.startsWithIgnoreCase ( "C64 Screens/" ) )
		{
			if ( fileChanged && ( file.hasFileExtension ( ".petmate" ) || file.hasFileExtension ( ".json" ) ) )
				mainScreen.pages.playerLayoutChanged ();

			return;
		}

		// Chip profile portrait mapping
		if ( parent.equalsIgnoreCase ( "UI/stil-names.csv" ) )
		{
			if ( ! fileChanged )
				return;

			stilLookup->load ();
		}

		// Data files
		if ( parent.startsWithIgnoreCase ( "Databases/" ) )
		{
			if ( ! fileChanged )
				return;

			if ( file.hasFileExtension ( ".csv" ) )
			{
				loadSIDPlayerProfilesAndOverrides ();
			}
			else if ( file.getFileName () == "STIL-addendum.txt" )
			{
				// The data only: setHVSCRoot's install check would switch pages
				hvscDatabase->load ( [ this ] { mainScreen.pages.setError ( juce::String ( hvscDatabase->getErrorString () ) ); } );
			}
			else if ( file.getFileName () == "tune-patches.txt" )
			{
				tunepatches::load ( datasource::loadText ( "Databases/tune-patches.txt" ) );

				// The length overrides live in the same file
				hvscDatabase->load ( [ this ] { mainScreen.pages.setError ( juce::String ( hvscDatabase->getErrorString () ) ); } );
			}

			return;
		}

		return;
	}

	//
	// Change happened to user-data folder
	//
	if ( file.isAChildOf ( roots.user ) )
	{
		auto	parent = file.getRelativePathFrom ( roots.user ).replaceCharacter ( '\\', '/' );

		// Tunes
		if ( parent.startsWithIgnoreCase ( "Tunes/" ) )
		{
			if ( ! file.hasFileExtension ( ".sid" ) )
				return;

			if ( event == gin::FileSystemWatcher::fileCreated || event == gin::FileSystemWatcher::fileRenamedNewName )
			{
				userDatabase->addUserTune ( file );
			}
			else if ( event == gin::FileSystemWatcher::fileDeleted || event == gin::FileSystemWatcher::fileRenamedOldName )
			{
				userDatabase->removeUserTune ( file );
			}
			else if ( event == gin::FileSystemWatcher::fileUpdated )
			{
				userDatabase->removeUserTune ( file );
				userDatabase->addUserTune ( file );
			}

			mainScreen.pages.setUserDatabase ( userDatabase->getAllEntries () );

			// The change freed/replaced Database::entry objects, every page
			// holding cached entry pointers must re-resolve them NOW, before
			// any paint or save touches a stale pointer
			mainScreen.refreshUserTunes ();

			mainScreen.pages.repaint ();
			return;
		}

		// User theme: no event filter, deleting the active theme's file must
		// also reload (Theme::load then falls back to the code defaults)
		if ( parent.startsWithIgnoreCase ( "Themes/" ) )
		{
			if ( file == theme->resolve ( preferences->get<juce::String> ( "ui/theme" ) ) )
				loadTheme ();

			return;
		}

		// Chip-profile overlay: the full load always runs factory-then-user, so
		// editing, creating and deleting the file all resolve the same way (a
		// deleted overlay reverts to factory values), hence no event filter
		if ( parent.equalsIgnoreCase ( "chip-profiles.csv" ) )
		{
			loadSIDPlayerProfilesAndOverrides ();
			return;
		}

		// User CRT presets are plain files read directly, no pak/lime coupling
		if ( parent.startsWithIgnoreCase ( "CRT Presets/" ) )
		{
			mainScreen.pages.userCRTPresetsChanged ();
			return;
		}

		// User CRT overlays and masks merge over the factory set through the
		// lime content loader
		if ( parent.startsWithIgnoreCase ( "Overlays/" ) || parent.startsWithIgnoreCase ( "CRT Masks/" ) )
		{
			mainScreen.pages.userCRTContentChanged ( parent, event );
			return;
		}

		return;
	}
}
//-----------------------------------------------------------------------------
