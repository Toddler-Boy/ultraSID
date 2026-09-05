#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Slider.h"
#include "ultra-shared/UI/Components/GUI_SVG_Button.h"

#include "Config/Preferences.h"
#include "UI/Components/GUI_QualitySelectorButton.h"

#include "GUI_QualitySelector.h"

//-----------------------------------------------------------------------------

class GUI_Volume : public juce::Component
{
public:
	GUI_Volume ();
	~GUI_Volume () override;

	// juce::Component
	void resized () override;
	void lookAndFeelChanged () override;

	// juce::MouseListener
	void mouseDown ( const juce::MouseEvent& event ) override;
	void mouseUp ( const juce::MouseEvent& event ) override;

	// this
	void changeVolume ( double delta );
	void updateQualityPosition ();
	void restorePreferences ();

	[[ nodiscard ]] const std::unordered_map<std::string, std::variant<int, float>> getState () const;

	GUI_QualitySelectorButton	quality { "quality", "REAL,PURE,MAGIC,EPIC,MYTHIC" };
	GUI_SVG_Button		mute { "mute", { "footer/volume/high" } };
	GUI_QualitySelector	qualitySelector;

private:
	juce::SharedResourcePointer<Preferences>	preferences;

	float	outVolume = 100.0f;
	float	lastDownVolume = 100.0f;

	GUI_Slider		volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

	void updateState ();
	void volumeChanged ();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Volume )
};
//-----------------------------------------------------------------------------
