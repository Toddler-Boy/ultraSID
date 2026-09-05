#include "Helpers/Messages.h"
#include "UI/GUI_SidebarRight.h"

#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

// Message-bus dispatch. Every verb from Helpers/Messages.h is routed through
// msg::Router (Helpers/MessageRouter.h); handlers are registered by role,
// navigation, transport, settings and artwork below, playlists in
// GUI_ultraSID_playlist.cpp, downloads/install and export in
// GUI_ultraSID_downloads.cpp.

void GUI_ultraSID::actionListenerCallback ( const juce::String& message )
{
	if ( message.isEmpty () )
		return;

	if ( ! router.dispatch ( message ) )
		Z_ERR ( "Unknown action: " << message );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::registerActions ()
{
	registerNavigationActions ();
	registerTransportActions ();
	registerSettingsActions ();
	registerArtworkActions ();
	registerShortcutActions ();
	registerPlaylistActions ();
	registerDownloadActions ();
	registerExportActions ();
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::registerNavigationActions ()
{
	router.on<msg::MainMenu> ( [ this ] ( const auto& e )
	{
		lastPage = e.name.toStdString ();
		showPage ( lastPage );
		saveState ();
	} );

	router.on<msg::ShowPage> ( [ this ] ( const auto& e )		{	showPage ( e.name.toStdString () );	} );
	router.on<msg::ShowPlaylist> ( [ this ] ( const auto& e )	{	showPlaylist ( e.name.toStdString () );	} );

	router.on<msg::ShowSearch> ( [ this ]	{	showPage ( "search" );	} );

	router.on<msg::ShowAbout> ( [ this ]
	{
		auto*	top = getTopLevelComponent ();
		auto	snapshot = top->createComponentSnapshot ( top->getLocalBounds () );

		mainScreen.pages.paintCRTIntoSnapshot ( snapshot, *top );
		aboutScreen.setBackground ( std::move ( snapshot ) );

		mainScreen.setVisible ( false );

		aboutScreen.setVisible ( true );
		aboutScreen.toFront ( true );
	} );

	router.on<msg::CloseAbout> ( [ this ]
	{
		aboutScreen.setVisible ( false );
		aboutScreen.setBackground ( nullptr );

		mainScreen.setVisible ( true );
	} );

	// The shortcut key toggles: showing while shown closes
	router.on<msg::ShowShortcuts> ( [ this ]
	{
		if ( shortcutsScreen.isVisible () )
			return msg::CloseShortcuts {}.send ();

		auto*	top = getTopLevelComponent ();
		auto	snapshot = top->createComponentSnapshot ( top->getLocalBounds () );

		mainScreen.pages.paintCRTIntoSnapshot ( snapshot, *top );
		shortcutsScreen.setBackground ( std::move ( snapshot ) );

		mainScreen.setVisible ( false );

		shortcutsScreen.setVisible ( true );
		shortcutsScreen.toFront ( true );
	} );

	router.on<msg::CloseShortcuts> ( [ this ]
	{
		shortcutsScreen.setVisible ( false );
		shortcutsScreen.setBackground ( nullptr );

		mainScreen.setVisible ( true );
	} );

	router.on<msg::GoToFolder> ( [ this ] ( const auto& e )		{	mainScreen.pages.setSearch ( e.folder, true );	} );
	router.on<msg::SetCRTPage> ( [ this ] ( const auto& e )		{	mainScreen.pages.loadGameArtwork ( e.page );	} );

	router.on<msg::SetLocation> ( [ this ] ( const auto& e )
	{
		if ( e.name == "hvsc" )
		{
			// The user explicitly picked a location, trust the spot checks again
			settings->set ( "hvsc/install-in-progress", false );
			settings->save ();

			setHVSCRoot ();
		}
		else if ( e.name == "user" )
			setUserRoot ();
		else
			Z_ERR ( "Unknown setLocation: " << e.name );

		const auto	isReady = areRootsValid ();

		mainScreen.sidebarLeft.enableMenus ( isReady );
		if ( ! onboardingScreen.isVisible () && ! isReady )
			showPage ( "settings" );
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::registerTransportActions ()
{
	router.on<msg::LoadTune> ( [ this ] ( const auto& e )		{	loadTune ( e.file, e.subtune, e.src, e.playlistPos );	} );
	router.on<msg::PlaySubtune> ( [ this ] ( const auto& e )	{	playSubtune ( e.subtune );	} );

	router.on<msg::Transport> ( [ this ] ( const auto& e )
	{
		if ( e.action == "play" )
		{
			togglePause ();
			updateTransportButtons ();
		}
		else if ( e.action == "prev" || e.action == "next" )
		{
			nextPreviousPlaylistItem ( e.action == "prev" ? -1 : 1, true );
			updateTransportButtons ();

			// Null when not playing from a playlist (deleted, root change)
			if ( auto pl = mainScreen.pages.getCurrentPlaylist () )
				pl->selectRow ( playQueue->playPosition );
		}
		else
			Z_ERR ( "Unknown transport: " << e.action );
	} );

	router.on<msg::PlayPlaylist> ( [ this ] ( const auto& e )
	{
		auto	newPlaylist = mainScreen.pages.getPlaylistItems ( e.name );
		if ( ! newPlaylist )
			return;

		mainScreen.pages.setCurrentPlaylist ( newPlaylist );

		// Start at the first playable row, empty playlists and leading
		// missing tunes (null entries) don't start anything
		for ( auto i = 0; i < newPlaylist->getSize (); ++i )
			if ( const auto item = newPlaylist->getItem ( i ) )
				return loadTune ( juce::String ( item->file.data (), item->file.size () ), newPlaylist->getSubtune ( i ), "playlist", i );
	} );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::registerSettingsActions ()
{
	router.on<msg::SettingChanged> ( [ this ] ( const auto& e )
	{
		if ( e.section == "ui" && e.key == "keep-screen-awake" )
			juce::Desktop::setScreenSaverEnabled ( ! preferences->get<bool> ( e.sectionKey () ) );
		else if ( e.section == "ui" && e.key == "theme" )
			loadTheme ();
		else if ( e.section == "fx" )
			updateFX ();
		else if ( e.section == "eq" )
			updateUserEQ ();
		else if ( e.section == "player" && e.key == "normalize" )
			player.setReplayGain ( preferences->get<bool> ( e.sectionKey () ) );
		else if ( e.section == "player" && e.key == "quality" )
			mainScreen.pages.refreshExportPreview ();	// {Q} follows the playback quality
		else if ( e.section == "player" && e.key == "boot-screen" )
		{
			mainScreen.pages.bootScreenPickChanged ();

			// The fallback thumbnail shows the same screen; the footer holds a
			// copy, so it gets the current tune's image re-pushed
			thumbnailCache->refreshDefaultImage ();
			updateFooterThumbnail ( lastFilename );

			mainScreen.repaint ();
		}
		else if ( e.section == "player" && e.key == "player-screen" )
			mainScreen.pages.playerScreenPickChanged ();
	} );

	router.on<msg::VolumeChanged> ( [ this ]	{	updateVolume ();	} );
	router.on<msg::RestoreState> ( [ this ]		{	restoreState ();	} );
	router.on<msg::TagsToggled> ( [ this ]		{	mainScreen.pages.repaint ();	} );
	router.on<msg::LikeChanged> ( [ this ]
	{
		mainScreen.sidebarRight.likeChanged ();

		// The hearts in every visible list
		mainScreen.pages.repaint ();
	} );
}
//-----------------------------------------------------------------------------

// Screenshot/artwork editing (developer mode)
void GUI_ultraSID::registerArtworkActions ()
{
	router.on<msg::AddScreenshots> ( [ this ] ( const auto& e )		{	addScreenshots ( e.files );	} );
	router.on<msg::AssignBorderColor> ( [ this ] ( const auto& e )	{	assignBorderColor ( e.index );	} );
	router.on<msg::RemoveBorderColor> ( [ this ]	{	assignBorderColor ( -1 );	} );
	router.on<msg::ToggleFirstLuma> ( [ this ]		{	toggleFirstLuma ();	} );
	router.on<msg::ToggleFirstLumaAll> ( [ this ]	{	toggleFirstLumaAll ();	} );
	router.on<msg::ToggleThumbnail> ( [ this ]		{	toggleThumbnail ();	} );
	router.on<msg::DeleteImage> ( [ this ]			{	deleteImage ();	} );
}
//-----------------------------------------------------------------------------
