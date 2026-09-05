#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// Single-line regex input with a history dropdown, the model-scoped force
// toggles and an add button. The last patterns are kept in the settings file,
// so test patterns don't have to be retyped between sessions

class PatternInputComponent final : public juce::Component
{
public:
	PatternInputComponent ();

	void resized () override;

	// Loads the pattern history from (and later saves it to) the given settings
	void setHistoryStorage ( juce::PropertiesFile* settingsIn );

	// Called with the trimmed pattern and the force states when the user adds one
	std::function<void ( const juce::String& pattern, bool force6581, bool force8580 )>	onAddPattern;

private:
	void addPattern ();
	void refreshHistoryItems ();

	static constexpr auto	maxHistoryEntries = 20;

	juce::ComboBox		patternBox;
	juce::ToggleButton	force6581Button { "Force 6581" };
	juce::ToggleButton	force8580Button { "Force 8580" };
	juce::TextButton	addButton { "Add to queue" };

	juce::PropertiesFile*	settings = nullptr;
	juce::StringArray		history;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( PatternInputComponent )
};
//-----------------------------------------------------------------------------
