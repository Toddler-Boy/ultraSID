#include <JuceHeader.h>

#include "GUI_ChipLogo.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_ChipLogo::GUI_ChipLogo ()
{
	setName ( "chipLogo" );
	setBufferedToImage ( true );
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_ChipLogo::paint ( juce::Graphics& g )
{
	g.setColour ( findColour ( UI::chipText ) );
	g.fillPath ( UI::getScaledPath ( model, getLocalBounds ().toFloat ().reduced ( 0.0f, yPadding ), juce::Justification::centredLeft ) );
}
//-----------------------------------------------------------------------------

void GUI_ChipLogo::setModel ( const std::string& _model )
{
	model = "logos/MOS" + _model;
	yPadding = ( _model == "6581" ) * 4.0f;

	repaint ();
}
//-----------------------------------------------------------------------------
