#include <JuceHeader.h>

#include "GUI_Volume.h"

#include "std_lime/lime_math.h"

#include "Helpers/Messages.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_Volume::GUI_Volume ()
{
	setName ( "volume" );

	// Quality button
	{
		quality.setTooltip ( "footer/quality" );
		quality.setClickingChangesState ( false );

		quality.onClick = [ this ]
  		{
			updateQualityPosition ();

			if ( qualitySelector.isOpen () )
				qualitySelector.close ();
			else
				qualitySelector.open ();
		};

		qualitySelector.qualityChanged = [ this ] ( const int q )
		{
			quality.setMultiStateInt ( q );

			const auto	fxState = quality.getMultiState ();
			preferences->set ( "player/quality", fxState );

			msg::SettingChanged { "player", "quality" }.send ();

			updateState ();
		};

		qualitySelector.setVisible ( true );
	}

	// Mute toggle
	{
		mute.margin = 6.0f;
		mute.placement = juce::RectanglePlacement::yMid;

		mute.setToggleable ( true );
		mute.setClickingTogglesState ( true );

		mute.onClick = [ this ]
		{
			const auto	outMuted = mute.getToggleState ();

			volume.setValue ( outMuted ? 0.0 : outVolume, juce::dontSendNotification );
			preferences->set ( "player/mute", outMuted );

			updateState ();
		};
	}

	// Volume slider
	{
		volume.setName ( "volume" );
		volume.setRange ( 0.0, 100.0, 1.0 );

		volume.onValueChange = [ this ]
		{
			volumeChanged ();
		};
	}

	addAndMakeVisible ( quality );
	addAndMakeVisible ( mute );
	addAndMakeVisible ( volume );

	volume.addMouseListener ( this, false );
}
//-----------------------------------------------------------------------------

GUI_Volume::~GUI_Volume ()
{
	volume.removeMouseListener ( this );
}
//-----------------------------------------------------------------------------

void GUI_Volume::restorePreferences ()
{
	// Quality
	quality.setMultiState ( preferences->get<juce::String> ( "player/quality" ) );

	// Mute
	mute.setToggleState ( preferences->get<bool> ( "player/mute" ), juce::dontSendNotification );

	// Volume
	lastDownVolume = float ( preferences->get<int> ( "player/volume" ) );
	outVolume = lastDownVolume;

	volume.setValue ( lastDownVolume, juce::NotificationType::dontSendNotification );

	updateState ();

	qualitySelector.setQuality ( quality.getStateInt () );
}
//-----------------------------------------------------------------------------

void GUI_Volume::resized ()
{
	updateQualityPosition ();
}
//-----------------------------------------------------------------------------

void GUI_Volume::lookAndFeelChanged ()
{
	qualitySelector.resized ();
	qualitySelector.repaint ();
}
//-----------------------------------------------------------------------------

void GUI_Volume::mouseDown ( const juce::MouseEvent& event )
{
	if ( event.eventComponent != &volume )
		return;

	if ( outVolume > 0.0f )
		lastDownVolume = outVolume;
}
//-----------------------------------------------------------------------------

void GUI_Volume::mouseUp ( const juce::MouseEvent& event )
{
	if ( event.eventComponent != &volume )
		return;

	if ( volume.getValue () < 1.0 )
	{
		mute.setToggleState ( true, juce::dontSendNotification );
		outVolume = lastDownVolume;

		preferences->set ( "player/mute", true );

		volumeChanged ();
	}
}
//-----------------------------------------------------------------------------

void GUI_Volume::changeVolume ( double delta )
{
	volume.setValue ( volume.getValue () + delta, juce::dontSendNotification );
	volumeChanged ();
}
//-----------------------------------------------------------------------------

void GUI_Volume::updateQualityPosition ()
{
	const auto	tl = quality.getScreenPosition ();

	auto&	qs = qualitySelector;
	qs.setTopRightPosition ( tl.x + 12, tl.y - qs.getHeight () );
}
//-----------------------------------------------------------------------------

void GUI_Volume::volumeChanged ()
{
	outVolume = float ( volume.getValue () );
	if ( outVolume >= 1.0f && mute.getToggleState () )
	{
		mute.setToggleState ( false, juce::dontSendNotification );
		preferences->set ( "player/mute", false );
	}

	preferences->set ( "player/volume", int ( outVolume ) );

	updateState ();
}
//-----------------------------------------------------------------------------

const std::unordered_map<std::string, std::variant<int, float>> GUI_Volume::getState () const
{
	return {
		{ "volume",		mute.getToggleState () ? 0.0f : outVolume },
		{ "quality",	quality.getStateInt () },
	};
}
//-----------------------------------------------------------------------------

void GUI_Volume::updateState ()
{
	// FX mode
	{
		quality.setColorId ( UI::colors::fxReal + quality.getStateInt () );
	}

	// Speaker icon
	{
		mute.setTooltip ( mute.getToggleState () ? "footer/volume/unmute" : "footer/volume/mute" );

		const static juce::StringArray	volStates = { "mute", "low", "medium", "high" };

		auto	vol = int ( lime::remap ( volume.getValue (), 0.0, 100.0, 0.99, 3.99 ) );
		if ( mute.getToggleState () )
			vol = 0;

		mute.svgNames.set ( 0, "footer/volume/" + volStates[ vol ] );
		mute.repaint ();
	}

	msg::VolumeChanged {}.send ();
}
//-----------------------------------------------------------------------------
