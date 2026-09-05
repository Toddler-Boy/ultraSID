#include <algorithm>
#include <array>

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"

#include "Helpers/Messages.h"

#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

// The user-facing keys are data (Data/UI/shortcuts.csv, verb per key) and
// dispatch through the router like any other message; this file owns the
// handlers behind those verbs. The modal Escape and the hidden developer
// combos stay ahead of the lookup, they are not in the list by design.

// Tab in any form, and the unmodified cursor keys; modified arrows are
// shortcuts (seek, volume, track)
static bool isNavigationKey ( const juce::KeyPress& key )
{
	if ( key.isKeyCode ( juce::KeyPress::tabKey ) )
		return true;

	if ( key.getModifiers ().isAnyModifierKeyDown () )
		return false;

	static const std::array	cursorKeys { juce::KeyPress::leftKey, juce::KeyPress::rightKey, juce::KeyPress::upKey, juce::KeyPress::downKey,
										 juce::KeyPress::homeKey, juce::KeyPress::endKey, juce::KeyPress::pageUpKey, juce::KeyPress::pageDownKey };

	return std::ranges::contains ( cursorKeys, key.getKeyCode () );
}
//-----------------------------------------------------------------------------

bool GUI_ultraSID::keyPressed ( const juce::KeyPress& key )
{
	// Only navigation keys arm the focus ring: a shortcut that moves the focus
	// (Ctrl+N) behaves like the click it stands in for
	if ( isNavigationKey ( key ) )
		focusRing.keyboardUsed ();

	// Tab with the focus on this component itself steps into the children
	if ( key.isKeyCode ( juce::KeyPress::tabKey ) && hasKeyboardFocus ( false ) )
	{
		if ( auto traverser = createKeyboardFocusTraverser () )
		{
			if ( const auto all = traverser->getAllComponents ( this ); ! all.empty () )
				( key.getModifiers ().isShiftDown () ? all.back () : all.front () )->grabKeyboardFocus ();
		}

		return true;
	}

	if ( key == juce::KeyPress ( juce::KeyPress::escapeKey, juce::ModifierKeys::noModifiers, 0 ) )
	{
		if ( aboutScreen.isVisible () )
			msg::CloseAbout {}.send ();
		else if ( shortcutsScreen.isVisible () )
			msg::CloseShortcuts {}.send ();
		else
			return false;

		return true;
	}

	if ( key == juce::KeyPress ( juce::KeyPress::F9Key, juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0 ) )
	{
		// The hidden chip-profile editor for sid-authors; no dev-mode gate,
		// its secrecy is the key combo itself
		toggleChipProfileEditor ();
	}
	else if ( key == juce::KeyPress ( juce::KeyPress::F10Key, juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0 ) )
	{
		// The hidden color-adjustment window; same secrecy-by-combo
		toggleColorAdjust ();
	}
#if ULTRA_INSPECTOR
	else if ( key == juce::KeyPress ( juce::KeyPress::F11Key, juce::ModifierKeys::noModifiers, 0 ) && ! mainScreen.pages.isCRTVisible () )
	{
		// Toggle inspector (F11 over the CRT is fullscreen)
		if ( ! inspector )
		{
			inspector = std::make_unique<melatonin::Inspector> ( *this );
			inspector->setRootFollowsComponentUnderMouse ( true );
			inspector->setVisible ( true );
			inspector->setAlwaysOnTop ( true );
			inspector->onClose = [ this ] { inspector = nullptr; };
		}
		else
		{
			inspector = nullptr;
		}
	}
#endif
	else if ( key == juce::KeyPress ( juce::KeyPress::F11Key, juce::ModifierKeys::shiftModifier, 0 ) )
	{
		auto&	laf = static_cast<GUI_LookAndFeel&> ( getLookAndFeel () );

		// Toggle log-window
		const lime::LoggerOptions opts {
			.name = "ultraSID",
			.settingsFolder = "ultraSID",
			.font = laf.monoFontPoints ( 12.3f, 600 ),
		};

		auto& lw = lime::Logger::getInstance ()->getLoggingWindow ( opts );
		lw.setVisible ( ! lw.isVisible () );
	}
	else if ( key == juce::KeyPress ( juce::KeyPress::F11Key, juce::ModifierKeys::commandModifier, 0 ) && buildinfo::isDeveloperMode () )
	{
		// Toggle peak-meters
		const auto	visible = ! inputMeter[ 0 ].isVisible ();

		inputMeter[ 0 ].setVisible ( visible );
		inputMeter[ 1 ].setVisible ( visible && player.getNumChips () > 1 );
		outputMeter[ 0 ].setVisible ( visible );
		outputMeter[ 1 ].setVisible ( visible );
	}
	else if ( const auto verb = shortcuts->find ( key ); verb.isNotEmpty () )
	{
		if ( ! router.dispatch ( verb ) )
			Z_ERR ( "Shortcut verb without handler: " << verb );
	}
	else
		return false;

	return true;
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::registerShortcutActions ()
{
	router.on<msg::TogglePlay> ( [ this ]		{	mainScreen.footer.togglePlay ();	} );
	router.on<msg::LikePlaying> ( [ this ]		{	toggleLikePlaying ();	} );
	router.on<msg::ToggleShuffle> ( [ this ]	{	mainScreen.footer.toggleShuffle ();	} );
	router.on<msg::CycleRepeat> ( [ this ]		{	mainScreen.footer.cycleRepeat ();	} );
	router.on<msg::PreviousTrack> ( [ this ]	{	mainScreen.footer.previousTrack ();	} );
	router.on<msg::NextTrack> ( [ this ]		{	mainScreen.footer.nextTrack ();	} );
	router.on<msg::SeekBack> ( [ this ]			{	mainScreen.footer.seekRelative ( -5.0 );	} );
	router.on<msg::SeekForward> ( [ this ]		{	mainScreen.footer.seekRelative ( 5.0 );	} );
	router.on<msg::VolumeUp> ( [ this ]			{	mainScreen.footer.changeVolume ( 5.0 );	} );
	router.on<msg::VolumeDown> ( [ this ]		{	mainScreen.footer.changeVolume ( -5.0 );	} );
	router.on<msg::ToggleMute> ( [ this ]		{	mainScreen.footer.toggleMute ();	} );
	router.on<msg::ToggleQuality> ( [ this ]	{	mainScreen.footer.toggleQualitySelector ();	} );
	router.on<msg::JumpToPlaying> ( [ this ]	{	jumpToPlayingTune ();	} );
	router.on<msg::ShowSettings> ( [ this ]		{	showPage ( "settings" );	} );
	router.on<msg::Undo> ( [ this ]				{	undoManager->undo ();	} );

	router.on<msg::NewPlaylist> ( [ this ]		{	mainScreen.sidebarLeft.clickAddPlaylist ();	} );

	router.on<msg::FocusSearch> ( [ this ]
	{
		showPage ( "search" );
		mainScreen.pages.showSearch ();
	} );

	router.on<msg::ShowLiked> ( [ this ]
	{
		showPage ( "search" );
		mainScreen.pages.showLiked ();
	} );

	router.on<msg::ShowPlaylists> ( [ this ]
	{
		showPage ( "playlists" );
		mainScreen.pages.showPlaylist ( "" );
		mainScreen.pages.setPage ( "playlists" );
	} );

	router.on<msg::ToggleFullscreen> ( [ this ]
	{
		if ( mainScreen.pages.isCRTVisible () )
			toggleFullscreen ();
	} );
}
//-----------------------------------------------------------------------------
