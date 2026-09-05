#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/Resources/Theme.h"

//-----------------------------------------------------------------------------

class GUI_MainMenuButton final : public juce::Button
{
public:

	GUI_MainMenuButton ( const juce::String& name );

	// juce::Component
	juce::MouseCursor getMouseCursor () override { return juce::MouseCursor::PointingHandCursor; }

	// this
	void setWorkCount ( const int count );
	void setErrorCount ( const int count );

	// Screen center of the work-badge slot (see paintButton's layout)
	[[ nodiscard ]] juce::Point<int> workBadgeScreenAnchor () const
	{
		return { getScreenX () + getWidth () - 8 - getHeight () / 2, getScreenBounds ().getCentreY () };
	}

protected:
	// juce::Component
	void enablementChanged () override;

	// juce::Button
	void paintButton ( juce::Graphics& g, bool hover, bool down ) override;

private:
	void setBadgeText ( const int count, juce::String& text );

	juce::SharedResourcePointer<Icons>		icons;
	juce::SharedResourcePointer<Strings>	strings;
	juce::SharedResourcePointer<Theme>		theme;

	juce::String	icon;
	juce::String	workText;
	juce::String	errorText;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_MainMenuButton )
};
//-----------------------------------------------------------------------------
