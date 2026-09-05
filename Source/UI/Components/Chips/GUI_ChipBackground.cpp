#include <JuceHeader.h>

#include "GUI_ChipBackground.h"

#include "ultra-shared/Config/DataSource.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_ChipBackground::GUI_ChipBackground ()
{
	setName ( "chipBackground" );
	setBufferedToImage ( true );
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_ChipBackground::paint ( juce::Graphics& g )
{
	if ( background.isNull () )
		background.setImage ( datasource::loadImage ( "UI/png/chip-background.png" ) );

	background.draw ( g, getLocalBounds ().toFloat () );

	// Indentations for the voices and the filter
	if ( const auto	divotCol = findColour ( UI::colors::chipDivot );
		 ! divotCol.isTransparent () && UI::lines::visible ( UI::lineWidth ( UI::lines::chip_divot ) ) )
	{
		const auto	filterR = getLocalArea ( indentationComponents[ 1 ], indentationComponents[ 1 ]->getLocalBounds ().toFloat () );
		const auto	voiceR = getLocalArea ( indentationComponents[ 0 ], indentationComponents[ 0 ]->getLocalBounds ().toFloat ().withHeight ( filterR.getHeight () ) );

		// The divots outline the chip-state boxes, so their rounding derives
		// from the same corner role, widened by the themed ring width
		const auto	divotW = UI::lineWidth ( UI::lines::chip_divot );
		const auto	divotRadius = UI::corner ( UI::corners::chip_states, voiceR ) + divotW / 2.0f;

		juce::Path	p;
		p.addRoundedRectangle ( voiceR.expanded ( divotW ), divotRadius );
		p.addRoundedRectangle ( filterR.expanded ( divotW ), divotRadius );

		if ( digiVisible )
		{
			const auto	digiR = getLocalArea ( indentationComponents[ 2 ], indentationComponents[ 2 ]->getLocalBounds ().toFloat () );
			p.addRoundedRectangle ( digiR.expanded ( divotW ), divotRadius );
		}

		g.setColour ( divotCol );
		g.fillPath ( p );
	}
}
//-----------------------------------------------------------------------------

void GUI_ChipBackground::addIndentation ( juce::Component* c )
{
	indentationComponents.emplace_back ( c );
}
//-----------------------------------------------------------------------------

void GUI_ChipBackground::setDigiVisible ( const bool shouldBeVisible )
{
	digiVisible = shouldBeVisible;
	repaint ();
}
//-----------------------------------------------------------------------------
