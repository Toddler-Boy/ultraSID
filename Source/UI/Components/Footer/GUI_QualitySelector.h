#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_Label.h"

#include "UI/Components/GUI_Popup.h"

//-----------------------------------------------------------------------------

// The quality popup: Up/Down move between the qualities, Enter/Space select

class GUI_QualitySelector final : public GUI_Popup
{
public:
	GUI_QualitySelector ();

	// juce::Component
	void resized () override;

	// this
	void setQuality ( const int quality );

	std::function<void ( const int )>	qualityChanged;

protected:
	// GUI_Popup
	bool popupKeyPressed ( const juce::KeyPress& key ) override;
	void focusOnOpen () override;

private:
	int	quality = 0;

	class QualityButton : public juce::ToggleButton
	{
	public:
		QualityButton ( const juce::String& _name, const int colorId );

		// juce::Component
		juce::MouseCursor getMouseCursor () override { return juce::MouseCursor::PointingHandCursor; }

	protected:
		// juce::ToggleButton
		void paintButton ( juce::Graphics& g, bool isHover, bool isDown ) override;

	private:
		int		colorId = 0;
		juce::SharedResourcePointer<Strings>	strings;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( QualityButton )
	};

	GUI_DynamicLabel		qualityLabel { "footer/quality/header", UI::fonts::popup_header };
	QualityButton			qButs[ 5 ] = {
		{ "real", 0 },
		{ "pure", 1 },
		{ "magic", 2 },
		{ "epic", 3 },
		{ "mythic", 4 }
	};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_QualitySelector )
};
//-----------------------------------------------------------------------------
