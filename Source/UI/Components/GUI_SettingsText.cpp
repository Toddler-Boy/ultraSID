#include "UI/Components/GUI_SettingsText.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "Helpers/Messages.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_SettingsText::GUI_SettingsText ( const juce::String& setSection, const juce::String& setName, const juce::String& layoutName )
	: juce::Component ( setSection + "-" + setName )
	, layoutFile ( "UI/layouts/components/" + layoutName + ".json" )
	, settingSection ( setSection )
	, settingName ( setName )
	, label ( "settings/" + setSection + "/" + setName, UI::fonts::settings_entry, UI::colors::text )
{
	help.setName ( "help" );
	help.setText ( strings->get ( ( "settings/" + setSection + "/" + setName + "-help" ).toStdString () ), UI::colors::textMuted );

	previewText.setName ( "preview" );

	text.applyFontToAllText ( UI::font ( UI::fonts::settings_field ), true );
	text.setJustification ( juce::Justification::centredLeft );
	text.setIndents ( 8, 0 );
	text.setBorder ( {} );

	text.onTextChange = [ this ] { refreshPreview (); };

	addAndMakeVisible ( label );
	addAndMakeVisible ( help );
	addAndMakeVisible ( previewText );
	addAndMakeVisible ( text );

	auto finishedEdit = [ this ]
	{
		text.onFocusLost ();
		giveAwayKeyboardFocus ();
	};

	text.onReturnKey = finishedEdit;
	text.onEscapeKey = finishedEdit;

	text.onFocusLost = [ this ]
	{
		// Actual changes are announced, for settings that apply live
		const auto	key = settingSection + "/" + settingName;
		const auto	value = text.getText ().trim ();

		text.setText ( value, juce::dontSendNotification );

		if ( value != preferences->get<juce::String> ( key ) )
		{
			preferences->set ( key, value );
			msg::SettingChanged { settingSection }.send ();
		}
	};
}
//-----------------------------------------------------------------------------

void GUI_SettingsText::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							layoutFile } );

	lookAndFeelChanged ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsText::lookAndFeelChanged ()
{
	const auto	txtCol = UI::getShade ( 1.0f );

	text.applyFontToAllText ( UI::font ( UI::fonts::settings_field ), true );

	text.setColour ( juce::TextEditor::backgroundColourId, UI::getShade ( 0.2f ) );
	text.setColour ( juce::TextEditor::textColourId, txtCol );
	text.applyColourToAllText ( txtCol );

	// An emptied field falls back to the preference's default, show it
	text.setTextToShowWhenEmpty ( preferences->getDefault<juce::String> ( settingSection + "/" + settingName ), findColour ( UI::colors::textMuted ) );
}
//-----------------------------------------------------------------------------

void GUI_SettingsText::restorePreference ()
{
	text.setText ( preferences->get<juce::String> ( settingSection + "/" + settingName ), juce::dontSendNotification );

	refreshPreview ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsText::setPreview ( std::function<std::optional<std::vector<GUI_ColorText::Segment>> ( const juce::String& )> fn )
{
	preview = std::move ( fn );

	refreshPreview ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsText::refreshPreview ()
{
	if ( ! preview )
		return;

	if ( auto sample = preview ( text.getText ().trim () ) )
		previewText.setSegments ( std::move ( *sample ) );
	else
		previewText.setText ( strings->get ( ( "settings/" + settingSection + "/" + settingName + "-invalid" ).toStdString () ), UI::colors::statusError );
}
//-----------------------------------------------------------------------------
