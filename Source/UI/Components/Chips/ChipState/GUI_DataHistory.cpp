#include <JuceHeader.h>

#include "GUI_DataHistory.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

//-----------------------------------------------------------------------------

void GUI_DataHistory::paint ( juce::Graphics& g )
{
	const auto	lineW = UI::lineWidth ( UI::lines::chip_states );

	// Draw current history
	const auto	b = UI::padded ( getLocalBounds ().toFloat (), UI::paddings::chip_history );

	auto	gs = GUI_RoundedClip ( g, b, 0.0f );

	const auto	db = b.reduced ( 0.0f, lineW );

	// The curve's newest sample ends at the dot, which rides lineW off the edge
	const auto	trans = juce::AffineTransform::scale ( db.getWidth () - lineW, -db.getHeight () ).translated ( db.getBottomLeft () );
	const auto	col = findColour ( colorId, true );

	constexpr auto	fillAlphaTop = 0.3f;
	constexpr auto	fillAlphaBottom = 0.075f;

	const auto	grad = juce::ColourGradient::vertical ( col.withMultipliedAlpha ( fillAlphaTop ), col.withMultipliedAlpha ( fillAlphaBottom ), db );

	g.setGradientFill ( grad );
	g.fillPath ( path, trans );

	g.setColour ( col );
	g.strokePath ( path, juce::PathStrokeType ( lineW ), trans );

	auto drawDot = [ &g, &db, col, lineW, this ] ( const float dot, const float saturation, const float alpha )
	{
		g.setColour ( col.withMultipliedSaturation ( saturation ).withMultipliedAlpha (alpha) );
		g.fillEllipse ( juce::Rectangle<float> { db.getRight () - lineW - dot * 0.5f, ( firstValue * -db.getHeight () ) + db.getBottom () - dot * 0.5f, dot, dot } );
	};

	drawDot ( lineW * 5.0f, 1.0f, 0.25f );
	drawDot ( lineW * 2.5f, 0.75f, 1.0f );
}
//-----------------------------------------------------------------------------

void GUI_DataHistory::setColorId ( const int colId )
{
	colorId = colId;
}
//-----------------------------------------------------------------------------

void GUI_DataHistory::reset ()
{
	lastResult = averageValue * historyXDelta;

	path.clear ();
	path.startNewSubPath ( 2.0f, -1.0f );

	firstPoint = true;
	currentX = 1.0f;
	averageValue = 0.0f;
}
//-----------------------------------------------------------------------------

void GUI_DataHistory::addDatapoint ( const float data )
{
	if ( firstPoint )
	{
		firstPoint = false;
		path.lineTo ( 2.0f, data );
		firstValue = data;
	}

	path.lineTo ( currentX, data );

	currentX -= historyXDelta;
	averageValue += data;
	lastValue = data;
}
//-----------------------------------------------------------------------------

void GUI_DataHistory::closePath ()
{
	path.lineTo ( -1.0f, lastValue );
	path.lineTo ( -1.0f, -1.0f );
	path.closeSubPath ();
}
//-----------------------------------------------------------------------------
