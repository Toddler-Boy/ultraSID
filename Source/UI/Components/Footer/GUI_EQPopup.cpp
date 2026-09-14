#include "GUI_EQPopup.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-corners.h"

//-----------------------------------------------------------------------------

namespace
{
	constexpr int	curveHeight = 120;
	constexpr int	boxPadding = 10;
}
//-----------------------------------------------------------------------------

GUI_EQPopup::GUI_EQPopup ()
	: GUI_Popup ( "eqPopup", true )
{
	constexpr auto	totalHeight = 20 + 5 + 24 + 10 + ( curveHeight + boxPadding * 2 );

	setSize ( 400 + shadowMargin * 2, totalHeight + shadowMargin * 2 );

	addAndMakeVisible ( header );
	addAndMakeVisible ( curve );
}
//-----------------------------------------------------------------------------

void GUI_EQPopup::resized ()
{
	GUI_Popup::resized ();

	auto	b = getPanelBounds ().reduced ( 10 );

	b.removeFromTop ( 5 );
	header.setBounds ( b.removeFromTop ( 24 ).translated ( 10, 0 ).withTrimmedRight ( 48 ) );
	b.removeFromTop ( 10 );

	curveBox = b.removeFromTop ( curveHeight + boxPadding * 2 ).toFloat ();
	curve.setBounds ( curveBox.toNearestInt ().reduced ( boxPadding ) );
}
//-----------------------------------------------------------------------------

void GUI_EQPopup::paint ( juce::Graphics& g )
{
	GUI_Popup::paint ( g );

	g.setColour ( UI::getShade ( GUI_EQCurve::boxBlend ) );
	g.fillRoundedRectangle ( curveBox, UI::corner ( UI::corners::settings_box, curveBox ) );
}
//-----------------------------------------------------------------------------
