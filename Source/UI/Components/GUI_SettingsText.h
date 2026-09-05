#pragma once

#include <JuceHeader.h>

#include <functional>
#include <optional>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_Label.h"

#include "Config/Preferences.h"
#include "UI/Components/GUI_ColorText.h"

//-----------------------------------------------------------------------------

// A free-text preference (with description text), stored trimmed when the
// edit finishes. Each instance names its layout json
class GUI_SettingsText final : public juce::Component
{
public:
	GUI_SettingsText ( const juce::String& setSection, const juce::String& setName, const juce::String& layoutName );

	// juce::Component
	void resized () override;
	void lookAndFeelChanged () override;

	// this
	void restorePreference ();

	// Optional live sample line under the edit, following every keystroke:
	// maps the current text to example segments; nullopt marks the text
	// invalid and shows the "<section>/<name>-invalid" string instead.
	// refreshPreview re-runs it when something the callback reads has changed
	void setPreview ( std::function<std::optional<std::vector<GUI_ColorText::Segment>> ( const juce::String& )> fn );
	void refreshPreview ();

	// Replaces the plain help text, for colorized token vocabularies
	void setHelpSegments ( std::vector<GUI_ColorText::Segment> segments )	{	help.setSegments ( std::move ( segments ) );	}

private:
	gin::LayoutSupport	layout { *this };

	juce::String	layoutFile;

	juce::SharedResourcePointer<Preferences>	preferences;
	juce::SharedResourcePointer<Strings>		strings;

	juce::String	settingSection;
	juce::String	settingName;

	GUI_DynamicLabel	label;
	GUI_ColorText		help;
	GUI_ColorText		previewText { UI::fonts::settings_help, juce::AttributedString::WordWrap::byChar };
	juce::TextEditor	text { "text" };

	std::function<std::optional<std::vector<GUI_ColorText::Segment>> ( const juce::String& )>	preview;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SettingsText )
};
//-----------------------------------------------------------------------------
