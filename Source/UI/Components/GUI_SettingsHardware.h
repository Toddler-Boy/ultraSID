#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_SolidButton.h"

#include "Audio/HardwareOutput.h"
#include "Config/Preferences.h"

//-----------------------------------------------------------------------------

// Hardware output settings body: the attached USBSID-Pico boards (tick to use, order
// sets the logical SID numbering), a live status line and what the mode switches off
class GUI_SettingsHardware final : public juce::Component
	, private juce::ListBoxModel
	, private juce::Timer
{
public:
	GUI_SettingsHardware ();
	~GUI_SettingsHardware () override;

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;
	void visibilityChanged () override;

private:
	struct Row
	{
		juce::String	serial;
		bool			attached = true;
		bool			selected = false;
	};

	// juce::ListBoxModel
	int getNumRows () override;
	void paintListBoxItem ( int row, juce::Graphics& g, int width, int height, bool rowIsSelected ) override;
	void listBoxItemClicked ( int row, const juce::MouseEvent& e ) override;

	// juce::Timer
	void timerCallback () override;

	void refresh ();
	void moveSelected ( int delta );
	void store ();
	[[ nodiscard ]] juce::String statusText () const;

	static constexpr int	rowHeight = 30;
	static constexpr int	checkWidth = 32;

	juce::SharedResourcePointer<Preferences>		preferences;
	juce::SharedResourcePointer<Strings>			strings;
	juce::SharedResourcePointer<HardwareOutput>		hardware;

	std::vector<Row>	rows;

	GUI_DynamicLabel	label;
	GUI_DynamicLabel	help;
	juce::ListBox		list { "boards", this };
	GUI_SolidButton		refreshButton { "refresh", "settings/hardware/refresh" };
	GUI_SolidButton		upButton { "up", "settings/hardware/up" };
	GUI_SolidButton		downButton { "down", "settings/hardware/down" };

	juce::String		shownStatus;

	juce::Rectangle<int>	statusBounds;
	juce::Rectangle<int>	disclosureBounds;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SettingsHardware )
};
//-----------------------------------------------------------------------------
