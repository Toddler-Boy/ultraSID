#include "GUI_SettingsInstall.h"

#include "ultra-shared/App/AppInstall.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_SettingsInstall::GUI_SettingsInstall ()
	: juce::Component ( "install" )
	, label ( "settings/install/title", UI::fonts::settings_entry, UI::colors::text )
	, help ( "settings/install/help", UI::fonts::settings_help, UI::colors::textMuted )
{
	help.setName ( "help" );

	addAndMakeVisible ( label );
	addAndMakeVisible ( help );
	addAndMakeVisible ( shortcutButton );
	addAndMakeVisible ( moveButton );
	addChildComponent ( status );

	// Already there: nothing to move
	moveButton.setEnabled ( ! appinstall::runsFromProgramsFolder () );

	shortcutButton.onClick = [ this ]	{	addShortcut ();	};
	moveButton.onClick = [ this ]		{	confirmMove ();	};
}
//-----------------------------------------------------------------------------

void GUI_SettingsInstall::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/components/settings-install.json" } );
}
//-----------------------------------------------------------------------------

void GUI_SettingsInstall::addShortcut ()
{
	if ( appinstall::createStartMenuShortcut () )
		setStatus ( GUI_SettingsLocationStatus::ok, strings->get ( "settings/install/shortcut-done" ) );
	else
		setStatus ( GUI_SettingsLocationStatus::error, strings->get ( "settings/install/shortcut-failed" ) );
}
//-----------------------------------------------------------------------------

void GUI_SettingsInstall::confirmMove ()
{
	juce::NativeMessageBox::showYesNoBox ( juce::MessageBoxIconType::QuestionIcon,
		strings->get ( "settings/install/move-title" ),
		strings->get ( "settings/install/move-message" ).replace ( "{}", appinstall::programsFolderExe ().getParentDirectory ().getFullPathName () ),
		this,
		juce::ModalCallbackFunction::create ( [ safe = juce::Component::SafePointer<GUI_SettingsInstall> ( this ) ] ( int r )
		{
			if ( r == 1 && safe != nullptr )
				safe->move ();
		} )
	);
}
//-----------------------------------------------------------------------------

void GUI_SettingsInstall::move ()
{
	if ( ! appinstall::moveToProgramsFolder () )
	{
		setStatus ( GUI_SettingsLocationStatus::error, strings->get ( "settings/install/move-failed" ) );
		return;
	}

	// The regular quit path, the moved program relaunches after the exit
	if ( auto* window = dynamic_cast<juce::DocumentWindow*> ( getTopLevelComponent () ) )
		window->closeButtonPressed ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsInstall::setStatus ( const GUI_SettingsLocationStatus::Status newStatus, const juce::String& message )
{
	status.setStatus ( newStatus, message );
	status.setVisible ( true );
}
//-----------------------------------------------------------------------------
