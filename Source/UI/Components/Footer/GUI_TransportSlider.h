#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Slider.h"

#include "UI/Components/GUI_ValueBubble.h"

//-----------------------------------------------------------------------------

class GUI_TransportSlider : public GUI_Slider
{
public:
	GUI_TransportSlider ();

	// this
	void setFont ( const juce::Font& font ) { popupDisplay.setFont ( font ); }
	void setLength ( int _lengthMS ) { lengthMS = _lengthMS; }

	// GUI_Slider
	void mouseEnter ( const juce::MouseEvent& e ) override;
	void mouseExit ( const juce::MouseEvent& e ) override;

	// juce::Slider
	void mouseDrag ( const juce::MouseEvent& e ) override;
	void mouseMove ( const juce::MouseEvent& e ) override;

private:
	GUI_ValueBubble		popupDisplay;

	int	lengthMS = 0;

	void showPopup ( const juce::MouseEvent& e );
	void hidePopup ();

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_TransportSlider )
};
//-----------------------------------------------------------------------------
