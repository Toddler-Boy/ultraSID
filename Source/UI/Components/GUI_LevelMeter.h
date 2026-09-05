#pragma once

#include <JuceHeader.h>

#include "Audio/LevelTracker.h"

//
// Thin vertical peak meter over a LevelTracker.
// These are the developer meters in the footer volume area, toggled with
// Ctrl+F11; the timer only runs while visible.
//
class GUI_LevelMeter final : public juce::Component, private juce::Timer
{
public:
	GUI_LevelMeter ( const LevelTracker& _tracker )
		: tracker ( _tracker )
	{
	}
	//-----------------------------------------------------------------------------

	void paint ( juce::Graphics& g ) override
	{
		const auto	bounds = getLocalBounds ().toFloat ();
		const auto	toFraction = [] ( const float db ) { return std::clamp ( ( db + rangeDb ) / rangeDb, 0.0f, 1.0f ); };

		g.setColour ( juce::Colours::black.withAlpha ( 0.4f ) );
		g.fillRect ( bounds );

		const auto	level = toFraction ( tracker.getLevel () );
		g.setColour ( tracker.getClip () ? juce::Colours::red : juce::Colours::limegreen );
		g.fillRect ( bounds.withTop ( bounds.getBottom () - bounds.getHeight () * level ) );

		// peak-hold marker, the maximum of the last two seconds
		if ( const auto hold = toFraction ( tracker.getHold () ); hold > 0.0f )
		{
			g.setColour ( juce::Colours::white );
			g.fillRect ( bounds.getX (), bounds.getBottom () - bounds.getHeight () * hold - 1.0f, bounds.getWidth (), 2.0f );
		}
	}
	//-----------------------------------------------------------------------------

	void visibilityChanged () override
	{
		if ( isVisible () )
			startTimerHz ( 30 );
		else
			stopTimer ();
	}
	//-----------------------------------------------------------------------------

private:
	void timerCallback () override	{	repaint ();	}

	static constexpr float	rangeDb = 60.0f;

	const LevelTracker&	tracker;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_LevelMeter )
};
//-----------------------------------------------------------------------------
