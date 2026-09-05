#include <typeinfo>

#include "GUI_ultraSID.h"

#include "ultra-shared/App/AppInstall.h"
#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/AssetTools.h"
#include "App/SharedProfiles.h"
#include "Config/FilePaths.h"
#include "Config/HVSCSource.h"
#include "Config/TunePatches.h"
#include "Data/Tags.h"
#include "Helpers/Messages.h"
#include "UI/GUI_ChipProfileEditor.h"
#include "UI/GUI_ColorAdjust.h"
#include "UI/ui-colors.h"

#include "GUI_AppLookAndFeel.h"

//-----------------------------------------------------------------------------

// AudioAppComponent only stores the reference here
GUI_ultraSID::GUI_ultraSID ()
	: juce::AudioAppComponent ( ownedDeviceManager )
	, mainScreen ( deviceManager )
{
	UI::setActionBroadCaster ( this );

	setName ( "ultraSID" );

	// Fallback target for editors handing back the keyboard focus, not a tab stop itself
	setWantsKeyboardFocus ( true );
	setFocusContainerType ( FocusContainerType::keyboardFocusContainer );

	juce::Desktop::getInstance ().addFocusChangeListener ( this );

	// The HVSC installer drives the install/update flows; the UI just renders
	hvscInstaller.onStatus = [ this ] ( const HVSCInstaller::Status status, const juce::String& message )
	{
		using enum HVSCInstaller::Status;
		setHVSCStatus ( status == ok ? GUI_SettingsLocationStatus::Status::ok
					  : status == warning ? GUI_SettingsLocationStatus::Status::warning
					  : GUI_SettingsLocationStatus::Status::error, message );

		// A failed download, extraction or post-install check must never
		// strand the user on a running progress page
		if ( status == error )
			resetInstallScreens ();
	};

	hvscInstaller.onInstallFinished = [ this ]	{	setHVSCRoot ();	};

	hvscInstaller.onCanceling = [ this ] ( const bool full )
	{
		full ? onboardingScreen.showCancelation () : updateHVSCScreen.showCancelation ();
	};

	hvscInstaller.onCanceled = [ this ] ( const bool full )
	{
		full ? onboardingScreen.startOver () : updateHVSCScreen.startOver ();
	};

	mainScreen.layout.setConstant ( "fullscreen", 0 );
	mainScreen.layout.setConstant ( "windowed", 1 );

	mainScreen.sidebarRight.setFFTSources ( fftMeasureLeft, fftMeasureRight );
	mainScreen.pages.setFFTSources ( fftMeasureLeft, fftMeasureRight );

	setWantsKeyboardFocus ( true );

	tooltipWindow->setOpaque ( false );

	// Undo toast: the manager decides, the toast renders and keeps the clock
	undoManager->onShow = [ this ] ( const juce::String& text )
	{
		undoToast.setMessage ( text );
		positionUndoToast ();

		undoToast.setVisible ( true );
		undoToast.toFront ( false );
		undoToast.startCountdown ( UndoManager::timeoutMS );
	};

	undoManager->onHide = [ this ]	{	undoToast.setVisible ( false );	};

	undoToast.onUndo = [ this ]		{	undoManager->undo ();	};
	undoToast.onExpired = [ this ]	{	undoManager->flush ();	};

	// Detect double-clicks on CRT images
	addMouseListener ( this, true );

	// A little trick to allow child components to send actions to this component
	addActionListener ( this );
	registerActions ();

	inputMeter[ 0 ].setName ( "inL" );
	inputMeter[ 1 ].setName ( "inR" );

	outputMeter[ 0 ].setName ( "outL" );
	outputMeter[ 1 ].setName ( "outR" );

	mainScreen.footer.attachMeter ( inputMeter[ 0 ] );
	mainScreen.footer.attachMeter ( inputMeter[ 1 ] );
	mainScreen.footer.attachMeter ( outputMeter[ 0 ] );
	mainScreen.footer.attachMeter ( outputMeter[ 1 ] );

	lastPage = "search";
	addAndMakeVisible ( mainScreen );
	addChildComponent ( onboardingScreen );
	addChildComponent ( updateHVSCScreen );
	addChildComponent ( aboutScreen );
	addChildComponent ( shortcutsScreen );
	addAndMakeVisible ( badgeOverlay );
	addAndMakeVisible ( focusRing );

	updateTransportButtons ();

	folderWatcher.coalesceEvents ( 50 );
	folderWatcher.addListener ( this );
	theme->setTargetLAF ( getLookAndFeel () );

	// Handle transport seek
	mainScreen.footer.onSeek ( [ this ] ( int newPosition )
	{
		player.seek ( newPosition );
	} );

	// The open quality selector has its own window, so the shortcuts come via callback
	mainScreen.footer.onQualitySelectorKey ( [ this ] ( const juce::KeyPress& key )
	{
		return keyPressed ( key );
	} );

	appUpdater.onStateChanged = [ this ] ( const AppUpdater::State state )
	{
		mainScreen.badge.version.setState ( state );
	};

	appUpdater.onProgress = [ this ] ( const float progress )
	{
		mainScreen.badge.version.setProgress ( progress );
	};

	// The regular quit path, the swapped-in exe relaunches after the exit
	appUpdater.onInstalled = [ this ]
	{
		static_cast<juce::DocumentWindow*> ( getParentComponent () )->closeButtonPressed ();
	};

	mainScreen.badge.version.onClick = [ this ]
	{
		if ( AppUpdater::canInstall && appUpdater.updatePending () )
			appUpdater.install ();
		else
			appUpdater.checkNow ();
	};
	mainScreen.badge.version.setState ( appUpdater.state () );

	appinstall::deleteStaleCopy ();

	setHVSCRoot ();
	setDataRoot ();
	setUserRoot ();

	// After setUserRoot (), which loads the update preferences
	appUpdater.check ();

	loadSIDPlayerProfilesAndOverrides ();

	// Show BASIC screen
	if ( areRootsValid () )
		mainScreen.pages.loadGameArtwork ( "" );

	updateVolume ();

	initAudio ();
}
//-----------------------------------------------------------------------------

GUI_ultraSID::~GUI_ultraSID ()
{
	juce::Desktop::getInstance ().removeFocusChangeListener ( this );

	// This component is the message-bus broadcaster, late sends must be dropped
	UI::setActionBroadCaster ( nullptr );

	// shutdownAudio () only detaches the callback from the app's own manager
	shutdownAudio ();
	deviceManager.closeAudioDevice ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::globalFocusChanged ( juce::Component* focused )
{
	focusRing.focusChanged ( focused );

	if ( ! buildinfo::isDeveloperMode () )
		return;

	if ( ! focused )
	{
		Z_INFO ( "Focus: none" );
		return;
	}

	// Class plus the path of component names down from the top level
	juce::String	path;
	for ( auto c = focused; c; c = c->getParentComponent () )
	{
		const auto	name = c->getName ().isNotEmpty () ? c->getName () : juce::String ( "?" );
		path = path.isEmpty () ? name : name + "/" + path;
	}

	Z_INFO ( "Focus: " << typeid ( *focused ).name () << " at " << path << " (order " << focused->getExplicitFocusOrder () << ")" );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toggleFullscreen ()
{
	const auto	kioskMode = isFullscreen ();

//	showPage ( kioskMode ? lastPage : "crt" );

	kioskMode ? toWindowed () : toFullscreen ();
}
//-----------------------------------------------------------------------------

bool GUI_ultraSID::isFullscreen () const
{
	return static_cast<juce::DocumentWindow*> ( getParentComponent () )->isKioskMode ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toFullscreen ()
{
	mainScreen.layout.setConstant ( "fullscreen", 1 );
	mainScreen.layout.setConstant ( "windowed", 0 );

	auto	parent = static_cast<juce::DocumentWindow*> ( getParentComponent () );

	// Only hide monitor/crt settings if they are open
	if ( ( settingsAreVisible = mainScreen.pages.areCRTSettingsVisible () ) )
		mainScreen.pages.showCRTSettings ( false );

	parent->parentHierarchyChanged ();

	juce::Desktop::getInstance ().setKioskModeComponent ( parent, false );

	wasFullscreen = true;
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toWindowed ()
{
	mainScreen.layout.setConstant ( "fullscreen", 0 );
	mainScreen.layout.setConstant ( "windowed", 1 );

	juce::Desktop::getInstance ().setKioskModeComponent ( nullptr, false );

	// Only show monitor/crt settings if they where open when toggling to fullscreen
	if ( settingsAreVisible )
		mainScreen.pages.showCRTSettings ( true );

	auto	parent = dynamic_cast<juce::DocumentWindow*> ( getTopLevelComponent () );
	parent->parentHierarchyChanged ();

	wasFullscreen = false;
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::moved ()
{
	mainScreen.footer.updateQualityPosition ();
	positionUndoToast ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::resized ()
{
	UI::setLayout ( layout, { "UI/layouts/screens.json" } );

	const auto	kioskMode = isFullscreen ();

	if ( wasFullscreen && ! kioskMode )
		toWindowed ();

	if ( ! kioskMode )
		mainScreen.footer.updateQualityPosition ();

	positionUndoToast ();
	badgeOverlay.setBounds ( getLocalBounds () );
	focusRing.setBounds ( getLocalBounds () );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::positionUndoToast ()
{
	if ( ! undoToast.isOnDesktop () )
		return;

	constexpr auto	margin = 12;
	const auto		footer = mainScreen.footer.getScreenBounds ();

	undoToast.setTopLeftPosition ( footer.getRight () - undoToast.getWidth () - margin,
								   footer.getY () - undoToast.getHeight () - margin );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::mouseDoubleClick ( const juce::MouseEvent& evt )
{
	// Double click on "CRT" toggles fullscreen
	if ( evt.eventComponent->getName () == "CRT" )
		toggleFullscreen ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::mouseDown ( const juce::MouseEvent& evt )
{
	// Click outside quality-selector hides it
	mainScreen.footer.dismissQualityOnOutsideClick ( evt.originalComponent,
													 evt.getEventRelativeTo ( this ).getScreenPosition () );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::parentHierarchyChanged ()
{
	if ( ! findParentComponentOfClass<juce::ResizableWindow> () )
		return;

	std::call_once ( firstStart, [ this ]
	{
		updateFooterThumbnail ( "" );
		loadTheme ();

		mainScreen.pages.updateGrid ();
		mainScreen.sidebarLeft.refreshPlaylists ();
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::update ( double time )
{
	// Calculate how much time has passed since last V-blank
	const auto	diff = std::min ( 1.0f, float ( time - lastTimer ) );
	lastTimer = time;

	badgeOverlay.animate ();

	//
	// Call various update functions
	//

	// The sidebar FFTs and the settings EQ share the measurements
	const auto	stereoAmount = dspEffects.outputStereoAmount ();
	const auto	leftChanged = fftMeasureLeft.update ();
	const auto	rightChanged = stereoAmount > 0.0f && fftMeasureRight.update ();

	if ( mainScreen.pages.isShowing () )
	{
		if ( mainScreen.pages.isCRTVisible () )
			mainScreen.pages.setCRTPlaybackTime ( int ( player.getTimeMS () ), player.getRenderLength (), int ( player.getRenderProgressMS () ) );

		mainScreen.pages.timerUpdate ( diff, player.getCPUCycles () );

		if ( mainScreen.sidebarRight.isShowing () )
		{
			mainScreen.sidebarRight.timerUpdate ( diff, stereoAmount, leftChanged, rightChanged );

			updateVoices ();
		}
		else if ( mainScreen.pages.isCRTVisible () )
			updateVoices ();
	}

	if ( mainScreen.footer.isShowing () )
		mainScreen.footer.setTransportTime ( player.getTimeMS (), player.getRenderProgressMS () );

	if ( leftChanged || rightChanged )
		mainScreen.pages.spectrumChanged ( stereoAmount > 0.0f );

	// Chip-profile editor loop region: the emulation can't seek backward, a
	// wrap re-renders from 0:00 with the current tweaks and resumes at start
	if ( chipEditor != nullptr && chipEditor->isLoopSet () && player.getTimeMS () >= chipEditor->getLoopEndMS () )
		restartTweakRender ( chipEditor->getLoopStartMS () );

	updatePlaylistPosition ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updateVoices ()
{
	if ( ! player.lockDigiBuffers () )
		return;

	const auto	numChips = player.getNumChips ();
	const auto	feedSidebar = mainScreen.sidebarRight.isShowing ();

	for ( auto chipIndex = 0; chipIndex < numChips; ++chipIndex )
 	{
 		const auto	[ regsPtr, regsIndex ] = player.getSidStatus ( chipIndex );

		// The count moves with the play position, same for all chips:
		// unchanged (pause, display faster than 60 Hz) = nothing new
		if ( chipIndex == 0 )
		{
			if ( regsIndex == lastSidDataCount )
				break;

			lastSidDataCount = regsIndex;

			// The CRT page's frequency strip runs on chip 0 alone
			if ( mainScreen.pages.isCRTVisible () )
				mainScreen.pages.setCRTVoiceRegs ( regsPtr, regsIndex );
		}

		if ( ! feedSidebar )
			break;

		mainScreen.sidebarRight.updateChip ( chipIndex, regsPtr, regsIndex );
		const auto	[ digiPtr, digiLookback ] = player.getDigiBuffer ( chipIndex );
		mainScreen.sidebarRight.setChipDigiData ( chipIndex, digiPtr, digiLookback );
	}

	player.unlockDigiBuffers ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updateVolume ()
{
	const auto	volState = mainScreen.footer.getVolumeState ();

	// FX
	const auto	fxMode = std::get<int> ( volState.at ( "quality" ) );
	dspEffects.setFXMode ( fxMode );

	// Volume
	dspEffects.setVolume ( std::get<float> ( volState.at ( "volume" ) ) * 0.01f );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updateFooterThumbnail ( const std::string& tunename )
{
	// The video standard comes from the tune here, as it does in the lists
	const auto	ent = db::findDatabaseEntry ( tunename );

	mainScreen.footer.setThumbnail ( thumbnailCache->getThumbnail ( tunename, ent && ent->isNTSC () ) );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updateColors ()
{
	const auto	bgCol = findColour ( UI::colors::window );
	const auto	textCol = findColour ( UI::colors::text );
	UI::setShades ( bgCol, textCol );

	const auto	darkCol = UI::getShade ( 0.1f );
	const auto	bento = bgCol;
	auto&	laf = getLookAndFeel ();

	laf.setColour ( juce::ResizableWindow::backgroundColourId, bgCol );
	laf.setColour ( juce::TextEditor::backgroundColourId, darkCol );
	laf.setColour ( juce::TextButton::buttonColourId, juce::Colours::orangered );

	laf.setColour ( UI::colors::bento, bento );
	laf.setColour ( UI::colors::textMuted, UI::getShade ( 0.5f ) );
	laf.setColour ( UI::colors::accentBright, UI::getColorWithPerceivedBrightness ( laf.findColour ( UI::colors::accent ), 0.6f ) );

	// Set some JUCE colors
	{
		laf.setColour ( juce::TooltipWindow::backgroundColourId, darkCol );

		laf.setColour ( juce::ScrollBar::backgroundColourId, UI::getShade ( 1.0f / 32.0f ) );
		laf.setColour ( juce::ScrollBar::thumbColourId, UI::getShade ( 0.25f ) );
		laf.setColour ( juce::ScrollBar::trackColourId, UI::getShade ( 0.5f ) );

		const auto	comboCol = UI::getShade ( 0.2f );

		laf.setColour ( juce::ComboBox::backgroundColourId, comboCol );
		laf.setColour ( juce::ComboBox::buttonColourId, comboCol );
		laf.setColour ( juce::ComboBox::textColourId, textCol );
		laf.setColour ( juce::ComboBox::arrowColourId, textCol );

		laf.setColour ( juce::PopupMenu::backgroundColourId, darkCol );
		laf.setColour ( juce::PopupMenu::textColourId, textCol );
		laf.setColour ( juce::PopupMenu::highlightedBackgroundColourId, UI::getShade ( 0.2f ) );

		if ( auto p = findParentComponentOfClass<juce::DocumentWindow> () )
		{
			p->setBackgroundColour ( bgCol );
			p->sendLookAndFeelChange ();
		}
	}

	mainScreen.pages.setCRTBackground ( bgCol );
	aboutScreen.updateColors ();

	// The FFTs resolve their colors from the theme per paint (setColorIds)
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::showPage ( const std::string& name )
{
	mainScreen.sidebarLeft.updateMenuState ( name );

	// Switch main page
	{
		const static juce::StringArray	mainPages {	"search", "playlists", "history", "crt", "export", "settings" };

		const auto	isOnMainScreen = mainPages.contains ( name );

		mainScreen.setVisible ( isOnMainScreen );
		if ( isOnMainScreen )
		{
			mainScreen.pages.setPage ( name );
			mainScreen.resized ();
		}
	}

	onboardingScreen.setVisible ( name == "onboarding" );
	updateHVSCScreen.setVisible ( name == "updateHVSC" );

	// Unknown page names (e.g. from a stale saved state) would show no screen at all
	if ( ! mainScreen.isVisible () && ! onboardingScreen.isVisible () && ! updateHVSCScreen.isVisible () )
		Z_ERR ( "Unknown page: " << juce::String ( name ) );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::showPlaylist ( const std::string& name )
{
	mainScreen.pages.showPlaylist ( name );
	mainScreen.sidebarLeft.selectPlaylist ( name );
	lastPage = "playlists";
	showPage ( "playlists" );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::loadTheme ()
{
	auto	themeName = preferences->get<juce::String> ( "ui/theme" );

	theme->setColorAdjustments ( colorAdjustmentsFromPreferences () );
	theme->load ( themeName );
	dynamic_cast<GUI_AppLookAndFeel&> ( getLookAndFeel () ).updateProgressColors ();

	updateColors ();
	sendLookAndFeelChange ();

	// The toast floats outside the component tree, reload it by hand
	if ( undoToast.isVisible () )
	{
		undoToast.refresh ();
		positionUndoToast ();
	}
}
//-----------------------------------------------------------------------------

Theme::ColorAdjustments GUI_ultraSID::colorAdjustmentsFromPreferences () const
{
	return {	float ( preferences->get<double> ( "ui/gamma" ) ),
				float ( preferences->get<double> ( "ui/brightness" ) ),
				float ( preferences->get<double> ( "ui/contrast" ) ),
				float ( preferences->get<double> ( "ui/saturation" ) ) };
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::updateColorAdjustments ()
{
	theme->setColorAdjustments ( colorAdjustmentsFromPreferences () );

	// Only colors changed, no metric: everything resolves them at paint time,
	// so the derived caches plus a repaint suffice, no lookAndFeelChanged
	// broadcast, which would re-flow layouts on every slider tick
	dynamic_cast<GUI_AppLookAndFeel&> ( getLookAndFeel () ).updateProgressColors ();
	updateColors ();
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toggleColorAdjust ()
{
	// The close click arrives from inside the window's own button callback,
	// delete it on the next message-loop tick
	auto close = [ this ] { juce::MessageManager::callAsync ( [ victim = colorAdjust.release () ] { delete victim; } ); };

	if ( colorAdjust != nullptr )
	{
		close ();
		return;
	}

	colorAdjust = std::make_unique<GUI_ColorAdjust> ( [ this ] { updateColorAdjustments (); }, close );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::applyPreferences ()
{
	updateFX ();
	updateUserEQ ();
	loadTheme ();

	// The default thumbnail was built before the preferences were loaded
	thumbnailCache->refreshDefaultImage ();
	updateFooterThumbnail ( lastFilename );

	juce::Desktop::setScreenSaverEnabled ( ! preferences->get<bool> ( "ui/keep-screen-awake" ) );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::loadSIDPlayerProfilesAndOverrides ()
{
	if ( roots.loadProfilesAndOverrides ( player ) )
		mainScreen.pages.repaintSearch ();

	mainScreen.showError ( roots.getProfileError () );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::setHVSCRoot ()
{
	// No installed at all
	installState->hvsc.versionInstalled = -1;

	// What the user picked is only a hint, the root is always the C64Music folder. Storing
	// the resolved one keeps the setting, the installer and the database on the same path
	const auto	chosen = juce::File ( settings->get<juce::String> ( "paths/hvsc" ) );

	roots.hvsc = AppRoots::resolveHVSCRoot ( chosen );

	if ( roots.hvsc != chosen )
	{
		settings->set ( "paths/hvsc", roots.hvsc.getFullPathName () );
		settings->save ();
	}

	// All HVSC reads go through the facade, folder or zip mode off this root
	hvscsource::setRoot ( roots.hvsc );
	tunepatches::load ( datasource::loadText ( "Databases/tune-patches.txt" ) );

	hvscInstaller.setRoot ( roots.hvsc );

	// Reported only when invalid: the valid path reports right below, and a
	// transient error here would make onStatus back out of the install screens
	if ( ! isHVSCRootValid () )
	{
		checkHVSCStatus ();
		return;
	}

	// Installed but not loaded yet; a stale error from an earlier attempt
	// must not shadow the fresh probe
	installState->hvsc.reset ();
	installState->hvsc.versionInstalled = 0;
	checkHVSCStatus ();

	// 0 means HVSC.txt is there but no release number parsed; load () below
	// would silently drop its callback on it, so treat it as the failure it is
	if ( roots.attachHVSCDatabase () <= 0 )
	{
		installState->hvsc.status = "Can't read HVSC version";
		checkHVSCStatus ();
		return;
	}

	//
	// Load HVSC databases (STIL & Bugs) in a background thread
	//
	hvscDatabase->load ( [ this ]
	{
		const auto	hvscError = juce::String ( hvscDatabase->getErrorString () );

		mainScreen.pages.setError ( hvscError );

		checkHVSCStatus ();

		msg::HvscCheck {}.send ();
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::setDataRoot ()
{
	// Which probe candidate won (the source resolves before the logger
	// exists, so the announcement lives here)
	Z_INFO ( "Data source: " << datasource::describe () );

	mainScreen.pages.setFileDatabase ( {} );

	folderWatcher.removeAllFolders ();

	tags->reload ();

	screenshots->reload ();
	thumbnailCache->reset ();
	stilLookup->load ();

	loadTheme ();

	loadROMs ();
	loadDatabase ();

	roots.loadSidIDConfig ( player );

	// Only development modifies factory data, and only the naked layout has
	// files to watch; the ultra-shared Data tree is the second factory root
	if ( buildinfo::isDeveloperMode () && ! datasource::isPak () )
	{
		folderWatcher.addFolder ( datasource::getDevFile () );
		folderWatcher.addFolder ( datasource::getSharedDevRoot () );
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::setUserRoot ()
{
	folderWatcher.removeFolder ( roots.user );

	mainScreen.pages.setUserDatabase ( {} );

	roots.user = settings->get<juce::String> ( "paths/user" );

	// User themes: created up front so users find the folder, set before
	// applyPreferences () below loads the theme
	if ( roots.user.isDirectory () )
	{
		auto	userThemes = roots.user.getChildFile ( "Themes" );

		userThemes.createDirectory ();
		theme->setUserRoot ( userThemes );
	}

	preferences->setRoot ( roots.user );
	mainScreen.footer.restoreVolumePreferences ();
	mainScreen.pages.restoreSettingsPreferences ();
	mainScreen.sidebarRight.restorePreferences ();

	applyPreferences ();

	likes->setRoot ( roots.user );

	userDatabase->scanUserTunes ();

	mainScreen.pages.setUserDatabase ( userDatabase->getAllEntries () );

	playlists->findPlaylists ();

	mainScreen.pages.setPlaylists ();

	{
		const auto	playlistNames = juce::SharedResourcePointer<Playlists> ()->getPlaylistNames ();

		mainScreen.sidebarLeft.setPlaylists ( playlistNames );
	}

	// After the user tunes are scanned, entries resolve to the database's keys
	history->setRoot ( roots.user );
	mainScreen.pages.loadHistory ();
	mainScreen.pages.loadExports ();
	mainScreen.pages.updateGrid ();
	mainScreen.refreshUserTunes ();

	folderWatcher.addFolder ( roots.user );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::loadDatabase ()
{
	roots.loadDatabase ( player );
	checkDatabaseStatus ();

	mainScreen.pages.setFileDatabase ( database->getAllEntries () );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::loadROMs ()
{
	roots.loadROMs ( player );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::saveState ()
{
	//
	// Save last page
	//
	settings->set ( "ui/page", juce::String ( lastPage ) );

	//
	// Last viewed CRT image
	//
	settings->set ( "ui/crt-image", mainScreen.pages.getCRTPage () );

	//
	// Playlists
	//
	{
		if ( auto visiblePlaylist = mainScreen.pages.getCurrentVisiblePlaylist () )
		{
			settings->set ( "ui/playlist", visiblePlaylist->getName () );
			settings->set ( "ui/playlist-selected", visiblePlaylist->getSelectedRow () );
			settings->set ( "ui/playlist-pos", visiblePlaylist->getVerticalPosition () );
		}
		else
		{
			settings->set ( "ui/playlist", std::string {} );
			settings->set ( "ui/playlist-selected", 0 );
			settings->set ( "ui/playlist-pos", 0.0 );
		}
	}

	//
	// Search
	//
	{
		auto&	results = mainScreen.pages.getSearchResults ();

		settings->set ( "ui/search-str", mainScreen.pages.getSearchString () );
		settings->set ( "ui/search-selected", results.getSelectedRow () );
		settings->set ( "ui/search-pos", results.getVerticalPosition () );

		auto&	header = results.getHeader ();

		settings->set ( "ui/search-col", header.getSortColumnId () );
		settings->set ( "ui/search-forwards", header.isSortedForwards () );
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::restoreState ()
{
	//
	// Restore playlist state
	//
	if ( auto playlistName = settings->get<juce::String> ( "ui/playlist" ); playlistName.isNotEmpty () )
	{
		showPlaylist ( playlistName.toStdString () );
		if ( auto curPls = mainScreen.pages.getCurrentVisiblePlaylist () )
		{
			curPls->selectRow ( settings->get<int> ( "ui/playlist-selected" ) );
			curPls->setVerticalPosition ( settings->get<double> ( "ui/playlist-pos" ) );
		}
	}

	//
	// Restore search state
	//
	{
		auto	searchStr = settings->get<juce::String> ( "ui/search-str" );
		mainScreen.pages.setSearch ( searchStr, false );

		auto&	results = mainScreen.pages.getSearchResults ();

		auto&	header = results.getHeader ();

		header.setSortColumnId ( settings->get<int> ( "ui/search-col" ),
								 settings->get<bool> ( "ui/search-forwards" ) );

		results.selectRow ( settings->get<int> ( "ui/search-selected" ) );
		results.setVerticalPosition ( settings->get<double> ( "ui/search-pos" ) );
	}

	//
	// Restore page
	//
	{
		auto	pageStr = settings->get<std::string> ( "ui/page" );

		if ( pageStr == "onboarding" || pageStr == "updateHVSC" )
			pageStr = "search";

		if ( ! isHVSCRootValid () )
		{
			pageStr = "onboarding";
			onboardingScreen.startOver ();
		}

		lastPage = pageStr;
		showPage ( pageStr );
	}

	mainScreen.pages.setCRTPage ( settings->get<int> ( "ui/crt-image" ) );

	applyPreferences ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::addScreenshots ( const juce::StringArray& filenames )
{
	if ( ! buildinfo::isDeveloperMode () )
		return;

	assettools::addScreenshots ( datasource::getDevFile (), lastFilename, filenames );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::addSidTunes ( const juce::StringArray& filenames )
{
	if ( filenames.isEmpty () )
		return;

	auto	dstPath = filepaths::getUserTunesPath ();
	if ( dstPath == juce::File () || ! dstPath.isDirectory () )
		return;

	// Move files
	for ( const auto& f : filenames )
	{
		auto	srcFile = juce::File ( f );
		if ( ! srcFile.hasFileExtension ( ".sid" ) || ! srcFile.existsAsFile () )
			continue;

		auto	dstFile = dstPath.getChildFile ( srcFile.getFileName () );
		srcFile.moveFileTo ( dstFile );
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::addPlaylistFiles ( const juce::StringArray& filenames )
{
	for ( const auto& f : filenames )
	{
		const auto	srcFile = juce::File ( f );
		if ( ! srcFile.hasFileExtension ( ".m3u" ) || ! srcFile.existsAsFile () )
			continue;

		const auto	imported = playlists->importPlaylist ( srcFile.getFileNameWithoutExtension (), srcFile.loadFileAsString () );
		if ( imported.name.isEmpty () )
		{
			Z_ERR ( "Not an M3U playlist: " << srcFile.getFullPathName () );
			continue;
		}

		// A cover next to the file, jpg first
		for ( const auto ext : { ".jpg", ".png" } )
			if ( const auto cover = srcFile.withFileExtension ( ext ); cover.existsAsFile () )
			{
				playlists->setPlaylistCover ( imported.name, cover.getFullPathName () );

				if ( ! imported.created )
					msg::PlaylistUpdateInfo { imported.name }.send ();

				break;
			}

		if ( imported.created )
			msg::PlaylistNew { imported.name }.send ();

		msg::ShowPlaylist { imported.name }.send ();
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::assignBorderColor ( const int index )
{
	assettools::setBorderColor ( mainScreen.pages.getLastLoadedArtwork (), index );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toggleFirstLuma ()
{
	assettools::toggleFirstLuma ( mainScreen.pages.getLastLoadedArtwork () );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toggleFirstLumaAll ()
{
	assettools::toggleFirstLumaAll ( datasource::getDevFile (), screenshots->getScreenshots ( lastFilename ) );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::toggleThumbnail ()
{
	assettools::toggleThumbnail ( mainScreen.pages.getLastLoadedArtwork () );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::deleteImage ()
{
	assettools::deleteImage ( mainScreen.pages.getLastLoadedArtwork () );
}
//-----------------------------------------------------------------------------
