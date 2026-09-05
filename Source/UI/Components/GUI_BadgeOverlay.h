#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Transparent full-window layer for the +N/-N pills that glide off grid
// tiles; the main window's vblank update drives the animation
class GUI_BadgeOverlay final : public juce::Component
{
public:
	GUI_BadgeOverlay ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void spawn ( const juce::Point<int> pos, const int delta );

	// Advances and repaints active badges, cheap no-op when idle
	void animate ();

private:
	struct Badge
	{
		juce::String			text;
		bool					positive;
		double					startMS;
		juce::Point<int>		anchor;
		juce::Rectangle<int>	box;
		juce::Rectangle<int>	lastArea;
		float					lastAlpha = 0.0f;
	};

	std::vector<Badge>	badges;

	[[ nodiscard ]] juce::Rectangle<int> areaFor ( const Badge& badge, float& alpha ) const;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_BadgeOverlay )
};
//-----------------------------------------------------------------------------
