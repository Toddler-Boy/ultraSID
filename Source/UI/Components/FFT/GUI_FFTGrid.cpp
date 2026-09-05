#include "GUI_FFTGrid.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

#include "fft-helpers.h"

//-----------------------------------------------------------------------------

// The reference frequencies and their caption texts
static const std::pair<float, juce::String> frequencies[] = {
	{ 15.625f, "16" },
	{ 31.25f,  "31" },
	{ 62.5f,   "62" },
	{ 125.0f, "125" },
	{ 250.0f, "250" },
	{ 500.0f, "500" },
	{ 1000.0f, "1k" },
	{ 2000.0f, "2k" },
	{ 4000.0f, "4k" },
	{ 8000.0f, "8k" },
};
//-----------------------------------------------------------------------------

void GUI_FFTGrid::paint ( juce::Graphics& g )
{
	// Vertical lines, clipped like the curves so they share the same shape
	if ( const auto gridCol = findColour ( UI::colors::fftGrid, true ); lines && ! gridCol.isTransparent () )
	{
		const auto	b = getLocalBounds ().toFloat ().withTrimmedBottom ( 6.0f );
		auto	sg = GUI_RoundedClip ( g, b, UI::corner ( UI::corners::fft_clip, b ) );

		g.setColour ( gridCol );

		for ( const auto& [ freq, txt ] : frequencies )
			g.fillRect ( juce::Rectangle<float> { UI::fft::freqToNormalized ( freq ) * float ( getWidth () ) - 0.25f, 0.0f, 0.5f, b.getHeight () } );
	}

	// Captions along the bottom edge
	if ( const auto textCol = findColour ( UI::colors::fftGridText, true ); captions && ! textCol.isTransparent () )
	{
		const auto	font = UI::font ( UI::fonts::fft_caption );
		g.setFont ( font );

		const auto	bckCol = findColour ( UI::bento, true ).withMultipliedAlpha ( 0.8f );

		for ( const auto& [ freq, txt ] : frequencies )
		{
			const auto	x = UI::fft::freqToNormalized ( freq ) * float ( getWidth () );

			const auto	strW = juce::GlyphArrangement::getStringWidth ( font, txt ) + 2.0f;
			const auto	strRect = juce::Rectangle<float> { x - strW / 2.0f, getHeight () - 10.0f, strW, 10.0f };

			// Darken background
			g.setColour ( bckCol );
			g.fillRect ( strRect );

			// Text
			g.setColour ( textCol );
			g.drawText ( txt, strRect, juce::Justification::centred, false );
		}
	}
}
//-----------------------------------------------------------------------------
