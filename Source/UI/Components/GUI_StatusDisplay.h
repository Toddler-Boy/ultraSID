#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"

#include "App/InstallState.h"

#include "GUI_ProgressBar.h"

//-----------------------------------------------------------------------------

class GUI_StatusDisplay final : public juce::Component, public juce::Timer
{
public:
	GUI_StatusDisplay ();

	// this
	void reset ();
	void showCancelation ();

	// juce::Timer
	void timerCallback() override;

private:
	juce::SharedResourcePointer<InstallState>	installState;

	GUI_Label			label { "", UI::fonts::onboarding_status };
	GUI_Label			progressStr { "", UI::fonts::onboarding_status };
	GUI_ProgressBar		progress;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_StatusDisplay )
};
//-----------------------------------------------------------------------------
