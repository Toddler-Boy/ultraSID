#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>
#include <vector>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_Checkbox.h"
#include "ultra-shared/UI/Components/GUI_Label.h"

#include "App/UserData.h"
#include "Config/Preferences.h"
#include "Config/Settings.h"
#include "Data/History.h"
#include "Data/Likes.h"
#include "UI/Components/GUI_SettingsLocationStatus.h"
#include "UI/Components/GUI_TextButton.h"

//-----------------------------------------------------------------------------

// Export/import of the user folder: one toggle per category found on disk,
// the buttons pack the checked ones into a zip or bring one back in
class GUI_SettingsUserData final : public juce::Component
{
public:
	GUI_SettingsUserData ();

	// juce::Component
	void resized () override;

	// this

	// Re-reads the user folder: counts and toggle availability
	void refresh ();

private:
	void exportArchive ();
	void importArchive ();
	void setStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message );

	[[ nodiscard ]] juce::File userRoot () const;
	[[ nodiscard ]] std::vector<userdata::Category> checkedCategories () const;

	gin::LayoutSupport	layout { *this };

	juce::SharedResourcePointer<Settings>		settings;
	juce::SharedResourcePointer<Preferences>	preferences;
	juce::SharedResourcePointer<Strings>		strings;
	juce::SharedResourcePointer<Likes>			likes;
	juce::SharedResourcePointer<History>		history;

	struct Row
	{
		std::unique_ptr<GUI_Checkbox>	toggle;
		std::unique_ptr<GUI_Label>	label;
	};

	std::array<Row, size_t ( userdata::Category::count )>	rows;

	GUI_DynamicLabel	label;
	GUI_DynamicLabel	help;
	GUI_TextButton		exportButton { "export", "settings/user-data/export" };
	GUI_TextButton		importButton { "import", "settings/user-data/import" };

	GUI_SettingsLocationStatus	status;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SettingsUserData )
};
//-----------------------------------------------------------------------------
