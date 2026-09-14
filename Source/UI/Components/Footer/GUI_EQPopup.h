#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"

#include "UI/Components/GUI_EQCurve.h"
#include "UI/Components/GUI_Popup.h"

class FFTMeasurement;

//-----------------------------------------------------------------------------

// The user tone curve as a footer popup

class GUI_EQPopup final : public GUI_Popup
{
public:
	GUI_EQPopup ();

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;

	// this
	void restorePreferences ()	{	curve.restorePreferences ();	}

	void setFFTSources ( const FFTMeasurement& left, const FFTMeasurement& right )	{	curve.setFFTSources ( left, right );	}
	void spectrumChanged ( const bool stereo )	{	curve.spectrumChanged ( stereo );	}

private:
	GUI_DynamicLabel	header { "settings/eq/label", UI::fonts::popup_header };
	GUI_EQCurve			curve;

	juce::Rectangle<float>	curveBox;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_EQPopup )
};
//-----------------------------------------------------------------------------
