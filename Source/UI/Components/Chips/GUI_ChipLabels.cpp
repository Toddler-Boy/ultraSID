#include <JuceHeader.h>

#include "GUI_ChipLabels.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_ChipLabels::GUI_ChipLabels ()
{
	setName ( "labels" );
	setBufferedToImage ( true );
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_ChipLabels::paint ( juce::Graphics& g )
{
	// Print legend under the fields
	g.setColour ( findColour ( UI::chipText ) );
	g.setFont ( UI::font ( UI::fonts::chip_labels ).withExtraKerningFactor ( 0.0f ).withHorizontalScale ( 0.9f ) );

	for ( const auto c : labels )
	{
		const auto	cb = getLocalArea ( c, c->getLocalBounds ().toFloat () );
		g.drawText ( c->getName ().toUpperCase (), cb.withY ( 0.0f ).withHeight ( float ( getHeight () ) ), juce::Justification::centred, false );
	}
}
//-----------------------------------------------------------------------------

void GUI_ChipLabels::addLabel ( juce::Component* c )
{
	labels.emplace_back ( c );
}
//-----------------------------------------------------------------------------
