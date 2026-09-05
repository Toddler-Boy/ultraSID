#include <JuceHeader.h>

#include "GUI_Voice.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

#include "../chip-constants.h"

//-----------------------------------------------------------------------------

GUI_Voice::GUI_Voice ()
{
	pitchDisplay.setColorId ( UI::colors::voiceOn );

	envelope.setName ( "vol" );
	envelope.setColorId ( UI::colors::voiceOn );
	envelope.setHistoryLength ( UI::chip::numHistory / 2 );

	addAndMakeVisible ( pitchDisplay );
	addAndMakeVisible ( control );
	addAndMakeVisible ( envelope );
	addAndMakeVisible ( oscTri );
	addAndMakeVisible ( oscSaw );
	addAndMakeVisible ( oscPls );
	addAndMakeVisible ( oscNse );
}
//-----------------------------------------------------------------------------

void GUI_Voice::paint ( juce::Graphics& g )
{
	const auto	col = findColour ( UI::colors::voiceOn, true );

	const auto	b = getLocalBounds ().toFloat ();
	const auto	radius = UI::corner ( UI::corners::chip_states, b );

	juce::Path	p;
	p.addRoundedRectangle ( b.getX (), b.getY (), b.getWidth (), b.getHeight (), radius, radius, roundTop, roundTop, roundBottom, roundBottom );

	g.setColour ( col.interpolatedWith ( juce::Colours::black, UI::chip::backBlack ) );
	g.fillPath ( p );
}
//-----------------------------------------------------------------------------

void GUI_Voice::lookAndFeelChanged ()
{
	setVoiceColors ();
}
//-----------------------------------------------------------------------------

void GUI_Voice::setState ( const int index, const uint8_t bits, const uint16_t pitch, const float note, const uint16_t pw, const bool _filtered, const bool _muted )
{
	pitchDisplay.reset ();
	envelope.reset ();

	control.setControl ( bits & 0x0F );

	oscTri.setState ( bits & 0x10, pitch, note, pw, 0 );
	oscSaw.setState ( bits & 0x20, pitch, note, pw, 0 );
	oscPls.setState ( bits & 0x40, pitch, note, pw, 0 );
	oscNse.setState ( bits & 0x80, pitch, note, pw, index );

	const auto	averageVol = std::clamp ( envelope.getAverage () * 20.0f - 0.05f, 0.0f, 1.0f );

	// Change colors depending on if the voice goes through the filter or not
	if ( _filtered != filtered || _muted != muted || ( std::abs ( averageVol - envLevel ) > ( 1.0f / 256.0f ) ) )
	{
		filtered = _filtered;
		muted = _muted;
		envLevel = averageVol;

		setVoiceColors ();

		repaint ();
	}
}
//-----------------------------------------------------------------------------

void GUI_Voice::setPitch ( const int index, const float note )
{
	pitchDisplay.addPitch ( note );

	if ( ! index )
	{
		pitchDisplay.closePath ();
		pitchDisplay.repaint ();
	}
}
//-----------------------------------------------------------------------------

void GUI_Voice::setEnvelope ( const int index, const uint8_t value )
{
	envelope.addDatapoint ( value * ( 1.0f / 255.0f ) );

	if ( ! index )
	{
		envelope.closePath ();
		envelope.repaint ();
	}
}
//-----------------------------------------------------------------------------

void GUI_Voice::setVoiceColors ()
{
	static int	voiceColorTable[ 4 ] = {
		UI::colors::voiceOn,			// not filtered
		UI::colors::filterOn,			// filtered
		UI::colors::voiceMuted,			// muted
		UI::colors::filterOn,			// muted, but filtered = audible
	};

	const auto	colIdx = int ( filtered ) + int ( muted ) * 2;

	const auto	txt = getLookAndFeel ().findColour ( voiceColorTable[ colIdx ] );
	const auto	off = getLookAndFeel ().findColour ( UI::colors::voiceOff );

	setColour ( UI::colors::voiceOn, off.interpolatedWith ( txt, envLevel ) );
}
//-----------------------------------------------------------------------------
