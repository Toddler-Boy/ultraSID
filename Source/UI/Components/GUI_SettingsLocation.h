#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_Label.h"

#include "Config/Preferences.h"
#include "Config/Settings.h"
#include "UI/Components/GUI_TextButton.h"

#include "GUI_SettingsLocationStatus.h"

//-----------------------------------------------------------------------------

class GUI_SettingsLocation final : public juce::Component
{
public:
	// canMove adds the button that carries the folder's content to a new place
	GUI_SettingsLocation ( const juce::String& setName, bool canMove = false );

	// juce::Component
	void resized () override;

	// this
	void hideBrowseButton ();
	void updateStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message );

	// Fires after a browsed path has been stored, for siblings that display it
	std::function<void ()>	onChanged;

private:
	// Copies the folder to dst, switches the setting over, then removes the old folder
	void moveTo ( const juce::File& src, const juce::File& dst );

	gin::LayoutSupport	layout { *this };

	juce::SharedResourcePointer<Settings>		settings;
	juce::SharedResourcePointer<Preferences>	preferences;
	juce::SharedResourcePointer<Strings>		strings;

	juce::String	settingName;

	GUI_DynamicLabel	label;
	GUI_Label			path;
	GUI_TextButton		browseButton { "browse", "settings/browse" };
	GUI_TextButton		moveButton { "move", "settings/move" };

	GUI_SettingsLocationStatus	status;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SettingsLocation )
};
//-----------------------------------------------------------------------------
