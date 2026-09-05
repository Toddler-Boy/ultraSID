#include <JuceHeader.h>

#include "GUI_VoiceControl.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_VoiceControl::GUI_VoiceControl ()
{
	setName ( "ctrl" );
}
//-----------------------------------------------------------------------------

void GUI_VoiceControl::paint (juce::Graphics& g)
{
	if ( ! value )
		return;

	g.setColour ( findColour ( UI::colors::voiceOn, true ) );

	constexpr auto	numBits = 4;
	constexpr auto	gap = 1.5f;

	auto	b = UI::padded ( getLocalBounds ().toFloat (), UI::paddings::chip_control );
	const auto	w = ( b.getWidth () + gap ) / numBits;

	for ( auto idx = 0; idx < numBits; ++idx )
	{
		const auto	r = b.removeFromLeft ( w );

		if ( value & ( 1 << idx ) )
			g.fillRect ( r.withWidth ( w - gap ) );
	}
}
//-----------------------------------------------------------------------------

void GUI_VoiceControl::setControl ( uint8_t val )
{
	if ( val == value )
		return;

	value = val;
	repaint ();
}
//-----------------------------------------------------------------------------
