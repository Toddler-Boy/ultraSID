#include <JuceHeader.h>

#include "GUI_ChipProfileIndicator.h"

//-----------------------------------------------------------------------------

GUI_ChipProfileIndicator::GUI_ChipProfileIndicator ()
{
	setName ( "chipProfileIndicator" );
	setDotColor ( false );
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileIndicator::paint ( juce::Graphics& g )
{
	g.setColour ( dotCol );
	g.fillEllipse ( getLocalBounds ().toFloat () );
}
//-----------------------------------------------------------------------------

void GUI_ChipProfileIndicator::setDotColor ( const bool hasProfile )
{
	dotCol = juce::Colour ( hasProfile ? 0xFF'93EFA4 : 0xFF'ECBF54 );
	repaint ();
}
//-----------------------------------------------------------------------------
