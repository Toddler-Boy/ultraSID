#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_SettingsLocationStatus : public juce::Component
{
public:
	GUI_SettingsLocationStatus ();

	// juce::Component
	void lookAndFeelChanged () override;
	void paint ( juce::Graphics& g ) override;

	// this
	enum Status
	{
		ok,
		warning,
		error
	};

	void setStatus ( const Status status, const juce::String& statusMsg );

private:
	Status			status = ok;
	juce::String	statusMessage;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SettingsLocationStatus )
};
//-----------------------------------------------------------------------------
