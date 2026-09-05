#include <JuceHeader.h>

#include "GUI_FilterMode.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_FilterMode::GUI_FilterMode ()
{
	setName ( "mode" );

	// Low-pass
	filterCurves[ 0 ].startNewSubPath ( 0.0f, 1.0f );
	filterCurves[ 0 ].lineTo ( 0.33f, 1.0f );
	filterCurves[ 0 ].lineTo ( 0.66f, 0.0f );
	filterCurves[ 0 ].lineTo ( 1.0f, 0.0f );

	// Band-pass
	filterCurves[ 1 ].startNewSubPath ( 0.0f, 0.0f );
	filterCurves[ 1 ].lineTo ( 0.1f, 0.0f );
	filterCurves[ 1 ].lineTo ( 0.33f, 1.0f );
	filterCurves[ 1 ].lineTo ( 0.66f, 1.0f );
	filterCurves[ 1 ].lineTo ( 0.9f, 0.0f );
	filterCurves[ 1 ].lineTo ( 1.0f, 0.0f );

	// High-pass
	filterCurves[ 2 ].startNewSubPath ( 0.0f, 0.0f );
	filterCurves[ 2 ].lineTo ( 0.33f, 0.0f );
	filterCurves[ 2 ].lineTo ( 0.66f, 1.0f );
	filterCurves[ 2 ].lineTo ( 1.0f, 1.0f );
}
//-----------------------------------------------------------------------------

void GUI_FilterMode::paint ( juce::Graphics& g )
{
	auto	b = UI::padded ( getLocalBounds ().toFloat (), UI::paddings::chip_filter_mode );

	constexpr auto	numModes = 3;

	// The gap between the mode curves is spacing, not padding: plain pixels
	constexpr auto	modeGap = 2.0f;

	const auto	cw = ( b.getWidth () - ( modeGap * ( numModes - 1 ) ) ) / numModes;

	for ( auto bit = 1; const auto& p : filterCurves )
	{
		const auto	colId = used ? UI::colors::filterOn : UI::colors::voiceOff;
		const auto	col = findColour ( colId );

		g.setColour ( col.withMultipliedAlpha ( ( mode & bit ) ? 1.0f : 0.2f ) );

		const auto	lr = b.removeFromLeft ( cw ).reduced ( bit == 2 ? 0.0f : 2.0f, 0.0f );
		b.removeFromLeft ( modeGap );

		g.strokePath ( p,
					   juce::PathStrokeType ( UI::lineWidth ( UI::lines::chip_states ), juce::PathStrokeType::JointStyle::mitered, juce::PathStrokeType::EndCapStyle::rounded ),
					   juce::AffineTransform::scale ( lr.getWidth (), -lr.getHeight () )
					   .translated ( lr.getBottomLeft () )
		);

		bit <<= 1;
	}
}
//-----------------------------------------------------------------------------

void GUI_FilterMode::setState ( const uint8_t _mode, const bool _used )
{
	const auto	needsRefresh = _mode != mode || _used != used;

	if ( ! needsRefresh )
		return;

	mode = _mode;
	used = _used;

	repaint ();
}
//-----------------------------------------------------------------------------
