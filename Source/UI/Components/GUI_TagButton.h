#pragma once

#include <JuceHeader.h>

#include "UI/ui-corners.h"
#include "UI/ui-fonts.h"
#include "UI/ui-lines.h"

class Icons;
class Strings;
class Theme;

//-----------------------------------------------------------------------------

class GUI_TagButton : public juce::ToggleButton
{
public:
	// The theme roles the button draws with; subclasses swap in their own
	// set to become separately themeable
	struct Roles
	{
		UI::fonts::Role		font = UI::fonts::tag_button;
		UI::corners::Role	corner = UI::corners::tag_button;
		UI::lines::Role		line = UI::lines::tag_button;
		UI::lines::Role		lineOn = UI::lines::tag_button_on;
	};

	GUI_TagButton ( const juce::String& name, const int colorId );
	GUI_TagButton ( const juce::String& name, const int colorId, const Roles _roles );

	// juce::Component
	void enablementChanged () override;
	void resized () override;
	void paint ( juce::Graphics& g ) override;

	// juce::TooltipClient
	juce::String getTooltip () override;

	// this
	void setButtonWidth ();

	// Optional off/on icon keys; empty = the button's name keys one icon for
	// both states
	juce::StringArray	stateIcons;

private:
	juce::SharedResourcePointer<Icons>		icons;
	juce::SharedResourcePointer<Strings>	strings;
	juce::SharedResourcePointer<Theme>		theme;

	const int			colorId;
	const Roles			roles;

	float	textWidth = 0.0f;
	float	textWidthOn = 0.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_TagButton )
};
//-----------------------------------------------------------------------------
