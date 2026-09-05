#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_FilterMode final : public juce::Component
{
public:
	GUI_FilterMode ();

	void paint ( juce::Graphics& g ) override;

	// this
	void setState ( const uint8_t mode, const bool used );

private:
	int		mode = 0;
	bool	used = false;

	std::array<juce::Path, 3>	filterCurves;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_FilterMode )
};
//-----------------------------------------------------------------------------
