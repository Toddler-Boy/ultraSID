#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_SettingsBox.h"

#include "UI/Components/GUI_ActionButton.h"
#include "UI/Components/GUI_SettingsLocation.h"
#include "UI/Components/GUI_StatusDisplay.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

// The two full-screen HVSC install flows: first-run onboarding (full
// download, choose locations) and version update. Same two-page structure,
// welcome/locations/start on page 1, progress/cancel on page 2, so one
// class, parameterized by mode.

class GUI_InstallScreen final : public juce::Component
{
public:
	enum class Mode : int8_t
	{
		onboarding,	// first-run full install
		update,		// update an existing collection
	};

	explicit GUI_InstallScreen ( const Mode mode );

	// juce::Component
	void resized () override;

	// this
	void setHVSCStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message );
	void showCancelation ();
	void startOver ();
	[[ nodiscard ]] bool isUpdating () const	{	return page2.isVisible ();	}

private:
	void start ();
	void stop ();

	const Mode	mode;

	gin::LayoutSupport	layout { *this };

	juce::Component	page1 { "page1" };
		juce::Label		header1 { "header" };

		GUI_SettingsBox	welcomeText { "welcome" };
			juce::Label		welcome { "label" };

		juce::Label	storageLabel { "label:storage" };
		GUI_SettingsBox	paths { "storage" };
 			GUI_SettingsLocation	hvscLocation { "hvsc" };
 			GUI_SettingsLocation	userLocation { "user" };	// onboarding only

		GUI_ActionButton	startButton;

	juce::Component	page2 { "page2" };
		juce::Label		header2 { "header" };

		GUI_SettingsBox	installingStatus { "installing" };
			GUI_StatusDisplay		statusDisplay;

		GUI_ActionButton	cancelButton;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_InstallScreen )
};
//-----------------------------------------------------------------------------
