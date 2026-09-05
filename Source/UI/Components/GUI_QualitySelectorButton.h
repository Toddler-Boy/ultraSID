#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"

//-----------------------------------------------------------------------------

class GUI_QualitySelectorButton : public juce::TextButton
{
public:
	GUI_QualitySelectorButton ( const juce::String& _name, const juce::String& _text );

	// juce::Component
	void enablementChanged () override;

	// juce::Button
	void paintButton ( juce::Graphics& g, bool isHover, bool isDown ) override;

	// juce::TooltipClient
	juce::String getTooltip () override;

	// this
	void setMultiState ( const juce::String& _state );
	[[ nodiscard ]] juce::String getMultiState () const;

	void setMultiStateInt ( const int _state );
	[[ nodiscard ]] int getStateInt () const { return currentState; }

	void setClickingChangesState ( bool _clickingChangesState ) { clickingChangesState = _clickingChangesState; }

	void setColorId ( const int colId ) { colorId = colId; }

protected:
	void clicked ( const juce::ModifierKeys& modifiers ) override;

private:
	juce::SharedResourcePointer<Strings>	strings;

	juce::StringArray	stateTexts;
	int		currentState = 0;
	bool	clickingChangesState = true;

	int		colorId = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_QualitySelectorButton )
};
//-----------------------------------------------------------------------------
