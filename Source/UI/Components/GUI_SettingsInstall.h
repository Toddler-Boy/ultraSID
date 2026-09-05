#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_SolidButton.h"

#include "UI/Components/GUI_SettingsLocationStatus.h"

//-----------------------------------------------------------------------------

// Windows only: a Start menu entry for the running exe, and the move of the
// exe into the per-user programs folder (which restarts the app from there)
class GUI_SettingsInstall final : public juce::Component
{
public:
	GUI_SettingsInstall ();

	// juce::Component
	void resized () override;

private:
	void addShortcut ();
	void confirmMove ();
	void move ();
	void setStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message );

	gin::LayoutSupport	layout { *this };

	juce::SharedResourcePointer<Strings>	strings;

	GUI_DynamicLabel	label;
	GUI_DynamicLabel	help;
	GUI_SolidButton		shortcutButton { "shortcut", "settings/install/shortcut" };
	GUI_SolidButton		moveButton { "move", "settings/install/move" };

	GUI_SettingsLocationStatus	status;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SettingsInstall )
};
//-----------------------------------------------------------------------------
