#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

class GUI_TextButton : public juce::Button
{
public:
	GUI_TextButton ( const juce::String& name, const juce::String& buttonText )
		: juce::Button ( name )
		, buttonText ( buttonText.toStdString () )
	{
	}
	//-------------------------------------------------------------------------

	void paintButton ( juce::Graphics& g, bool isHover, bool isDown ) override
	{
		const auto	lineWidth = UI::lineWidth ( UI::lines::settings_location_button );

		const auto	b = getLocalBounds ().toFloat ().reduced ( lineWidth / 2.0f );

		g.setColour ( findColour ( isHover ? UI::colors::text : UI::colors::textMuted ) );
		GUI_LookAndFeel::drawOutline ( g, getLocalBounds ().toFloat (), getHeight () / 2.0f, lineWidth );

		g.setColour ( findColour ( UI::colors::text ) );
		g.setFont ( UI::font ( UI::fonts::settings_location_button ) );
		g.drawText ( strings->get ( buttonText ), b, juce::Justification::centred, false );
	}
	//-------------------------------------------------------------------------

	void enablementChanged () override
	{
		setAlpha ( isEnabled () ? 1.0f : 0.5f );
	}
	//-------------------------------------------------------------------------

	juce::MouseCursor getMouseCursor () override
	{
		return isEnabled () ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::ParentCursor;
	}

private:
	juce::SharedResourcePointer<Strings>	strings;
	std::string		buttonText;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_TextButton )
};
//-----------------------------------------------------------------------------
