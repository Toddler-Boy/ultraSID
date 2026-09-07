#include <JuceHeader.h>

#include "GUI_PortraitPlaceholder.h"

#include "ultra-shared/UI/UI_Helpers.h"

//-----------------------------------------------------------------------------

void GUI_PortraitPlaceholder::draw ( juce::Graphics& g, const juce::Rectangle<float>& b, const juce::Colour& tint )
{
	g.setColour ( tint.withMultipliedAlpha ( 0.33f ) );
	g.fillRect ( b );
	g.setColour ( tint.withMultipliedAlpha ( 0.66f ) );
	g.fillPath ( UI::getScaledPathWithSize ( icons->get ( "portrait-unknown" ), b.translated ( 0.0f, b.getHeight () * 0.1f ), juce::RectanglePlacement::centred ) );
}
//-----------------------------------------------------------------------------
