#include <JuceHeader.h>

#include "GUI_InstallScreen.h"

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

// Per-mode configuration, everything the two flows differ in

namespace
{
	struct modeConfig
	{
		const char*	componentName;
		const char*	headerKey1;
		const char*	welcomeKey;
		const char*	headerKey2;
		const char*	layoutJson;

		const char*	startLabel;
		const char*	startTooltip;
		const char*	cancelLabel;
		const char*	cancelTooltip;

		msg::DownloadHVSC::action	startAction;
		msg::DownloadHVSC::action	cancelAction;
	};

	constexpr modeConfig	onboardingConfig {
		"onboarding",
		"install/welcome_header", "onboarding_welcome", "install/installing_header",
		"UI/layouts/screens/onboarding.json",
		"Download HVSC for me", "Download and set up the High Voltage SID Collection automatically.",
		"Cancel HVSC installation", "Cancel and delete unfinished installation.",
		msg::DownloadHVSC::action::full, msg::DownloadHVSC::action::cancel,
	};

	constexpr modeConfig	updateConfig {
		"updateHVSC",
		"install/update_header", "update_hvsc_welcome", "install/updating_header",
		"UI/layouts/screens/update-hvsc.json",
		"Update HVSC for me", "Update the High Voltage SID Collection automatically.",
		"Cancel HVSC update", "Cancel HVSC update",
		msg::DownloadHVSC::action::update, msg::DownloadHVSC::action::cancelUpdate,
	};

	const modeConfig& configFor ( const GUI_InstallScreen::Mode mode )
	{
		return mode == GUI_InstallScreen::Mode::onboarding ? onboardingConfig : updateConfig;
	}
}
//-----------------------------------------------------------------------------

GUI_InstallScreen::GUI_InstallScreen ( const Mode _mode )
	: juce::Component ( configFor ( _mode ).componentName )
	, mode ( _mode )
	, startButton ( "download", configFor ( _mode ).startLabel, UI::colors::statusOk, configFor ( _mode ).startTooltip )
	, cancelButton ( "cancel", configFor ( _mode ).cancelLabel, UI::colors::statusError, configFor ( _mode ).cancelTooltip )
{
	const juce::SharedResourcePointer<Strings>	strings;
	const auto&	config = configFor ( mode );

	//
	// Page 1
	//
	{
		// Header
		header1.setText ( strings->get ( config.headerKey1 ), juce::NotificationType::dontSendNotification );
		UI::setFontRole ( header1, UI::fonts::page_title );
		header1.setJustificationType ( juce::Justification::topLeft );
		page1.addAndMakeVisible ( header1 );

		// Welcome message
		welcome.setText ( strings->get ( config.welcomeKey ), juce::NotificationType::dontSendNotification );
		UI::setFontRole ( welcome, UI::fonts::onboarding_text );
		welcome.setJustificationType ( juce::Justification::topLeft );
		welcomeText.addAndMakeVisible ( welcome );
		page1.addAndMakeVisible ( welcomeText );

		// Locations
		storageLabel.setText ( strings->get ( "settings/header/storage" ), juce::NotificationType::dontSendNotification );
		UI::setFontRole ( storageLabel, UI::fonts::settings_section );
		page1.addAndMakeVisible ( storageLabel );
		paths.addAndMakeVisible ( hvscLocation );

		if ( mode == Mode::onboarding )
			paths.addAndMakeVisible ( userLocation );
		else
			hvscLocation.hideBrowseButton ();

		page1.addAndMakeVisible ( paths );

		// Download/update button
		page1.addAndMakeVisible ( startButton );

		startButton.onClick = [ this, &config ]
		{
			page1.setVisible ( false );
			page2.setVisible ( true );

			start ();

			msg::DownloadHVSC { config.startAction }.send ();
		};
	}

	//
	// Page 2
	//
	{
		// Header
		header2.setText ( strings->get ( config.headerKey2 ), juce::NotificationType::dontSendNotification );
		UI::setFontRole ( header2, UI::fonts::page_title );
		header2.setJustificationType ( juce::Justification::topLeft );
		page2.addAndMakeVisible ( header2 );

		// Installing status
		page2.addAndMakeVisible ( installingStatus );

		// Progress bar
		installingStatus.addAndMakeVisible ( statusDisplay );

		// Cancel button
		page2.addAndMakeVisible ( cancelButton );

		cancelButton.onClick = [ this, &config ]
		{
			if ( mode == Mode::onboarding )
				stop ();

			cancelButton.setEnabled ( false );
			msg::DownloadHVSC { config.cancelAction }.send ();
		};
	}

	addAndMakeVisible ( page1 );
	addChildComponent ( page2 );
}
//-----------------------------------------------------------------------------

void GUI_InstallScreen::resized ()
{
	UI::setLayout ( layout, juce::StringArray {	"UI/layouts/constants.json",
												configFor ( mode ).layoutJson } );
}
//-----------------------------------------------------------------------------

void GUI_InstallScreen::setHVSCStatus ( GUI_SettingsLocationStatus::Status status, const juce::String& message )
{
	hvscLocation.updateStatus ( status, message );
}
//-----------------------------------------------------------------------------

void GUI_InstallScreen::showCancelation ()
{
	statusDisplay.showCancelation ();
}
//-----------------------------------------------------------------------------

void GUI_InstallScreen::startOver ()
{
	stop ();

	page1.setVisible ( true );
	page2.setVisible ( false );

	statusDisplay.reset ();
	cancelButton.setEnabled ( true );
}
//-----------------------------------------------------------------------------

void GUI_InstallScreen::start ()
{
	statusDisplay.startTimerHz ( 30 );
}
//-----------------------------------------------------------------------------

void GUI_InstallScreen::stop ()
{
	statusDisplay.stopTimer ();
}
//-----------------------------------------------------------------------------
