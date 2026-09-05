#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_ProgressBar final : public juce::Component
{
public:
	GUI_ProgressBar ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void setProgress ( float progress ) noexcept;

private:
	float	currentProgress = 0.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ProgressBar )
};
//-----------------------------------------------------------------------------
