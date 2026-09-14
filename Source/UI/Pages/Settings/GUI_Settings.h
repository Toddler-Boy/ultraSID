#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_ViewportSmoothScroll.h"

#include "Config/Settings.h"
#include "UI/ComponentFactory.h"
#include "UI/Components/GUI_SettingsLocationStatus.h"

//-----------------------------------------------------------------------------

class GUI_Settings final : public juce::Component
{
public:
	GUI_Settings ( juce::AudioDeviceManager& adm );
	~GUI_Settings () override;

	// juce::Component
	void resized () override;

	// this
	void restorePreferences ();
	void refreshExportPreview ();
	void setHVSCStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message );

private:
	void updateDisablers ();

	juce::SharedResourcePointer<Settings>	settings;

	std::unordered_map<juce::String, juce::Component*> componentMap;

	gin::LayoutSupport	layout { *this, [] ( const juce::String& typeName ) { return componentFactory ( typeName ); } };

	juce::AudioDeviceManager&	adm;

	juce::Viewport	settingsVP { "viewport" };
	juce::Component	settingsWrapper { "wrapper" };
	GUI_ViewportSmoothScroll	smoothScroll { settingsVP };

	GUI_SettingsLocationStatus::Status	hvscStatus;
	juce::String						hvscStatusMessage;

	// Boot- and player-screen drop-down values, item id - 1 indexes them
	// ("Random" leads the player list)
	juce::StringArray	bootScreens;
	juce::StringArray	playerScreens;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Settings )
};
//-----------------------------------------------------------------------------
