#include "FooterComponent.h"

#include "CustomLookAndFeel.h"

//-----------------------------------------------------------------------------

void FooterComponent::setStats ( const Stats& newStats )
{
	stats = newStats;
	repaint ();
}
//-----------------------------------------------------------------------------

void FooterComponent::paint ( juce::Graphics& g )
{
	g.fillAll ( getShade ( 0.0f ) );

	const auto	font = juce::Font ( juce::FontOptions ( 13.0f ) );

	g.setColour ( getLookAndFeel ().findColour ( juce::ListBox::textColourId ) );
	g.setFont ( font );

	auto formatTime = [] ( const int64_t ms )
	{
		const auto	totalSecs = ms / 1000;
		return juce::String::formatted ( "%d:%02d:%02d", int ( totalSecs / 3600 ), int ( totalSecs / 60 ) % 60, int ( totalSecs % 60 ) );
	};

	auto	area = getLocalBounds ().reduced ( 12, 0 );

	// Draws a stat into a cell sized for the widest text it can show, so the
	// following cells don't shift as digit counts change
	auto drawStat = [ & ] ( const juce::String& text, const juce::String& widthTemplate )
	{
		const auto	width = int ( std::ceil ( juce::GlyphArrangement::getStringWidth ( font, widthTemplate ) ) );
		g.drawText ( text, area.removeFromLeft ( width ), juce::Justification::centredLeft, false );
		area.removeFromLeft ( 12 );
	};

	const auto	totalStr = juce::String ( stats.total );
	drawStat ( "Queue: " + juce::String ( stats.remaining ) + "/" + totalStr,
			   "Queue: " + totalStr + "/" + totalStr );

	const auto	longestTime = formatTime ( std::max ( stats.elapsedMs, stats.totalTimeMs ) );

	auto	timeText = "Time: " + formatTime ( stats.elapsedMs );
	auto	timeTemplate = "Time: " + longestTime;
	if ( stats.totalTimeMs >= 0 )
	{
		timeText += "/" + formatTime ( stats.totalTimeMs );
		timeTemplate += "/" + longestTime;
	}

	drawStat ( timeText, timeTemplate );

	// Total progress bar, fixed width, showing just the percentage. Clipping to
	// the rounded outline and filling plain rects keeps the corners intact even
	// when the fill is narrower than the corner radius
	{
		const auto	barCell = area.removeFromLeft ( 133 ).toFloat ();
		auto	barArea = barCell.reduced ( 0.0f, 4.0f );

		if ( barArea != barPath.getBounds () )
		{
			barPath.clear ();
			barPath.addRoundedRectangle ( barArea, barArea.getHeight () / 2.0f );
		}

		{
			const juce::Graphics::ScopedSaveState	state ( g );

			g.reduceClipRegion ( barPath );

			// Draw the filled portion of the bar, clipped to the rounded outline
			g.setColour ( getLookAndFeel ().findColour ( juce::ProgressBar::foregroundColourId ) );
			g.fillRect ( barArea.removeFromLeft ( barArea.getWidth () * float ( stats.fractionDone ) ) );

			// Track pinned to a fixed palette shade, identical on any background
			if ( barArea.getWidth () > 0.0f )
			{
				g.setColour ( getShade ( 0.04f ) );
				g.fillRect ( barArea );
			}
		}

		g.setColour ( getLookAndFeel ().findColour ( juce::ListBox::textColourId ) );
		g.drawText ( juce::String ( stats.fractionDone * 100.0, 2 ) + "%", barCell, juce::Justification::centred, false );
	}

	// Rendering speed: how much faster than real time each tune gets measured,
	// and the total across the currently active workers
	if ( stats.speedRatio >= 0.0 )
	{
		auto	speedText = juce::String ( stats.speedRatio, 2 ) + "x";
		if ( stats.totalSpeedRatio > 0.0 )
			speedText += " - " + juce::String ( stats.totalSpeedRatio, 2 ) + "x total";

		g.drawText ( speedText, area.withTrimmedLeft ( 12 ), juce::Justification::centredLeft, false );
	}
}
//-----------------------------------------------------------------------------
