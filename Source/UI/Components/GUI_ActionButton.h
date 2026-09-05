#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_ActionButton final : public juce::TextButton
{
public:
	GUI_ActionButton ( const juce::String& name, const juce::String& icon, const int colorId, const juce::String& tooltip = {} );

	// juce::Component
	void enablementChanged () override;
	void paint ( juce::Graphics& g ) override;
	juce::MouseCursor getMouseCursor () override { return juce::MouseCursor::PointingHandCursor; }

private:
	const juce::String	icon;
	const int			colorId;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ActionButton )
};
//-----------------------------------------------------------------------------
