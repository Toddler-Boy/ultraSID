#include "GUI_AlertBanner.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_AlertBanner::GUI_AlertBanner ()
	: juce::Component ( "alertBanner" )
{
	setVisible ( false );

	// Whatever it covers stays usable
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_AlertBanner::paint ( juce::Graphics& g )
{
	g.fillAll ( findColour ( UI::colors::statusError ) );

	const auto	newline = message.indexOfChar ( '\n' );
	const auto	what = newline < 0 ? message : message.substring ( 0, newline );
	const auto	sourceLine = newline < 0 ? juce::String () : message.substring ( newline + 1 );

	auto	bounds = getLocalBounds ().reduced ( 14, 8 ).toFloat ();

	g.setColour ( juce::Colours::white );

	g.setFont ( UI::font ( UI::fonts::alert_banner ) );

	g.drawText ( what, bounds.removeFromTop ( 22 ), juce::Justification::centredLeft, true );
	g.drawText ( sourceLine, bounds, juce::Justification::centredLeft, true );
}
//-----------------------------------------------------------------------------

void GUI_AlertBanner::setMessage ( const juce::String& msg )
{
	if ( message == msg )
		return;

	message = msg;

	setVisible ( message.isNotEmpty () );
	repaint ();
}
//-----------------------------------------------------------------------------
