#include "GUI_ProgressBar.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_ProgressBar::GUI_ProgressBar ()
	: juce::Component ( "progress" )
{
}
//-----------------------------------------------------------------------------

void GUI_ProgressBar::paint ( juce::Graphics& g )
{
	const auto	col = findColour ( UI::colors::statusOk );

	auto	b = getLocalBounds ().toFloat ();

	// Progress bar
	{
		auto	bar = b.removeFromLeft ( b.getWidth () - 60.0f );

		auto	rc = GUI_RoundedClip ( g, bar, bar.getHeight () * 0.5f );

		const auto	width = float ( bar.getWidth () * currentProgress );

		g.setColour ( col );
		g.fillRect ( bar.removeFromLeft ( width ) );

		g.setColour ( col.withMultipliedAlpha ( 0.1f ) );
		g.fillRect ( bar );
	}

	b.removeFromLeft ( 10.0f );

	// Text
	{
		g.setColour ( col );
		g.setFont ( UI::font ( UI::fonts::onboarding_progress ) );
		g.drawText ( juce::String ( int ( currentProgress * 100.0 ) ) + "%", b, juce::Justification::centred );
	}
}
//-----------------------------------------------------------------------------

void GUI_ProgressBar::setProgress ( float progress ) noexcept
{
	progress = std::clamp ( progress, 0.0f, 1.0f );
	if ( juce::approximatelyEqual ( currentProgress, progress ) )
		return;

	currentProgress = progress;
	repaint ();
}
//-----------------------------------------------------------------------------
