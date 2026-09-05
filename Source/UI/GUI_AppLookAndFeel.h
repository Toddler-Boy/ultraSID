#pragma once

#include "ultra-shared/UI/GUI_LookAndFeel.h"

//-------------------------------------------------------------------------------------------------

// The shared look plus the player pieces: the progress transport slider and
// the playback animation
class GUI_AppLookAndFeel final : public GUI_LookAndFeel
{
public:
	GUI_AppLookAndFeel ()
	{
		progressSlider.isRadial = false;
	}

	static void drawPlaybackAnimation ( juce::Graphics& g, const juce::Rectangle<float>& rect, const juce::Colour color, const float animSpeed );

	void updateProgressColors ();

	// OkLch-interpolated colors from start to end (both included), staying
	// saturated instead of dipping through sRGB gray
	[[ nodiscard ]] static std::vector<juce::Colour> oklchPalette ( juce::Colour startColor, juce::Colour endColor, int numSteps );

	// The gradient with every pair of neighboring stops bridged by OkLch
	// in-between colors
	[[ nodiscard ]] static juce::ColourGradient withOklchStops ( const juce::ColourGradient& gradient, int stepsPerSegment = 8 );

	// juce::LookAndFeel_V4
	void drawLinearSlider ( juce::Graphics&, int x, int y, int width, int height,
							float sliderPos, float minSliderPos, float maxSliderPos,
							const juce::Slider::SliderStyle, juce::Slider& ) override;

private:
	juce::ColourGradient	progressSlider;
	juce::Image				progressSliderImage;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_AppLookAndFeel )
};
//-------------------------------------------------------------------------------------------------
