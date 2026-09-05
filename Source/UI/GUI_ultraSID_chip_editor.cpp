#include "App/SharedProfiles.h"

#include "GUI_ChipProfileEditor.h"
#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

// The hidden chip-profile editor (Ctrl+Shift+F9): while it is open the render
// thread runs throttled just ahead of the playhead, so slider changes pushed
// into the live emulation become audible within ~100 ms. The emulation cannot
// seek, so both opening the window and wrapping the loop region re-render from
// 0:00 (a fast, silent sprint) and resume at the wanted position.

void GUI_ultraSID::toggleChipProfileEditor ()
{
	if ( chipEditor != nullptr )
	{
		closeChipProfileEditor ();
		return;
	}

	chipEditor = std::make_unique<GUI_ChipProfileEditor> (
		player,
		roots.user.getChildFile ( "chip-profiles.csv" ),
		[ this ] { closeChipProfileEditor (); } );

	player.setLiveTweak ( true );

	chipEditor->refresh ( currentChipSettings () );

	// Re-render throttled, playback continues right where it is
	restartTweakRender ( player.getTimeMS () );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::closeChipProfileEditor ()
{
	// The render sprints to the end; tweaked values stay audible until the
	// next tune load restores the CSV profile
	player.setLiveTweak ( false );

	if ( chipEditor == nullptr )
		return;

	// The close click arrives from inside the window's own button callback,
	// delete it on the next message-loop tick
	juce::MessageManager::callAsync ( [ victim = chipEditor.release () ] { delete victim; } );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID::restartTweakRender ( const uint32_t resumeMS )
{
	if ( lastFilename.empty () || ! player.isReadyToPlay () )
		return;

	disableAudio ();

	player.stopRender ();

	// initSong would re-populate the editor from the CSV profile; the flag
	// keeps the tweaked values in charge for a same-tune restart
	inTweakRestart = true;
	const auto	ready = initSong ( lastSong, true );
	inTweakRestart = false;

	if ( ready )
	{
		renderSong ();
		player.seek ( resumeMS );
	}

	enableAudio ();
}
//-----------------------------------------------------------------------------

SIDPlayer::ChipSettings GUI_ultraSID::currentChipSettings () const
{
	const auto&	info = player.getFileInfo ();

	// Emu-based editor tunes get fixed engine-side values, mirror them (only
	// the two that differ from the struct defaults)
	if ( juce::String ( info.chipProfile ).startsWith ( "emu-" ) )
	{
		SIDPlayer::ChipSettings	s;

		s.name = info.chipProfile;
		s.flt0Dac = 0.5;
		s.fltGain = 1.0;

		return s;
	}

	if ( const auto cfg = profiles->getPlayerConfig () )
		if ( const auto* tuneInfo = player.getSidTune ().getInfo () )
			return cfg->chipSelector.getProfile ( tuneInfo->path (), tuneInfo->dataFileName (), int ( info.currentSong ) );

	return {};
}
//-----------------------------------------------------------------------------
