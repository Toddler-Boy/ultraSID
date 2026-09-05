#include <JuceHeader.h>

#include "GUI_ValueBubble.h"

#include "ultra-shared/UI/UI_Helpers.h"

//-----------------------------------------------------------------------------

GUI_ValueBubble::GUI_ValueBubble ()
{
	setSize ( 40, 22 );
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_ValueBubble::paint ( juce::Graphics& g )
{
	const auto	b = getLocalBounds ().toFloat ();

	g.setColour ( UI::getShade ( 1.0f ) );
	g.fillRoundedRectangle ( b, UI::corner ( UI::corners::value_bubble, b ) );

	g.setFont ( font );
	g.setColour ( UI::getShade ( 0.0f ) );
	g.drawText ( text, b, juce::Justification::centred, false );
}
//-----------------------------------------------------------------------------
