#include "GUI_SettingsLocation.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "App/UserData.h"
#include "Config/Settings.h"
#include "Helpers/Messages.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_SettingsLocation::GUI_SettingsLocation ( const juce::String& setName, const bool canMove )
	: juce::Component ( setName )
	, settingName ( setName )
	, label ( "settings/location/" + setName, UI::fonts::settings_location, UI::colors::text)
	, path ( "", UI::fonts::settings_location, UI::colors::textMuted )
{
	path.setText ( settings->get<juce::String> ( "paths/" + settingName ) );
	path.setName ( "path" );

	addAndMakeVisible ( label );
	addAndMakeVisible ( path );
	addChildComponent ( status );
	addAndMakeVisible ( browseButton );

	auto browseFolder = [ this ] ( const juce::String& pathName, const juce::String& curPath, const juce::String& name )
	{
		auto	chooser = std::make_shared<juce::FileChooser> ( pathName, curPath, "*" );

		constexpr auto	chooserFlags = juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::openMode;
		chooser->launchAsync ( chooserFlags, [ this, chooser, name ] ( const juce::FileChooser& )
		{
			auto	dst = chooser->getResult ();
			if ( ! dst.isDirectory () )
				return;

			if ( settings->get<juce::String> ( "paths/" + name ) == dst.getFullPathName () )
				return;

			settings->set ( "paths/" + name, dst.getFullPathName () );

			path.setText ( settings->get<juce::String> ( "paths/" + settingName ) );

			msg::SetLocation { name }.send ();

			if ( onChanged )
				onChanged ();
		} );
	};

	browseButton.onClick = [ =, this ]
	{
		browseFolder ( strings->get ( "settings/location/" + settingName.toStdString () ), path.getText (), settingName );
	};

	addChildComponent ( moveButton );
	moveButton.setVisible ( canMove );

	moveButton.onClick = [ this ]
	{
		auto	chooser = std::make_shared<juce::FileChooser> ( strings->get ( "settings/move" ), path.getText (), "*" );

		constexpr auto	chooserFlags = juce::FileBrowserComponent::canSelectDirectories | juce::FileBrowserComponent::openMode;
		chooser->launchAsync ( chooserFlags, [ this, chooser ] ( const juce::FileChooser& )
		{
			const auto	picked = chooser->getResult ();
			const auto	src = juce::File ( settings->get<juce::String> ( "paths/" + settingName ) );

			if ( ! picked.isDirectory () )
				return;

			// A folder picked by its parent (Documents, Dropbox) gets the
			// data in a child of the usual name
			const auto	dst = picked.getFileName ().equalsIgnoreCase ( Settings::userDataFolderName () ) ? picked : picked.getChildFile ( Settings::userDataFolderName () );

			if ( dst == src )
				return;

			if ( dst.isAChildOf ( src ) || dst.getNumberOfChildFiles ( juce::File::findFilesAndDirectories ) > 0 )
			{
				updateStatus ( GUI_SettingsLocationStatus::error, strings->get ( "settings/move-not-empty" ) );
				return;
			}

			juce::NativeMessageBox::showYesNoBox ( juce::MessageBoxIconType::WarningIcon,
				strings->get ( "settings/move-title" ),
				strings->get ( "settings/move-message" ).replace ( "{}", dst.getFullPathName () ),
				this,
				juce::ModalCallbackFunction::create ( [ safe = juce::Component::SafePointer<GUI_SettingsLocation> ( this ), src, dst ] ( int r )
				{
					if ( r == 1 && safe != nullptr )
						safe->moveTo ( src, dst );
				} )
			);
		} );
	};
}
//-----------------------------------------------------------------------------

void GUI_SettingsLocation::moveTo ( const juce::File& src, const juce::File& dst )
{
	// Pending preference edits would otherwise be flushed into the old
	// folder after its copy was taken
	preferences->save ();

	if ( ! userdata::copyFolder ( src, dst ) )
	{
		updateStatus ( GUI_SettingsLocationStatus::error, strings->get ( "settings/move-failed" ) );
		return;
	}

	settings->set ( "paths/" + settingName, dst.getFullPathName () );
	path.setText ( dst.getFullPathName () );

	msg::SetLocation { settingName }.send ();

	if ( onChanged )
		onChanged ();

	// The switch above is queued, the old folder goes once it is no longer watched
	juce::MessageManager::callAsync ( [ safe = juce::Component::SafePointer<GUI_SettingsLocation> ( this ), src ]
	{
		const auto	removed = src.deleteRecursively ();

		if ( safe != nullptr )
			safe->updateStatus ( removed ? GUI_SettingsLocationStatus::ok : GUI_SettingsLocationStatus::warning,
								 safe->strings->get ( removed ? "settings/move-done" : "settings/move-old-kept" ) );
	} );
}
//-----------------------------------------------------------------------------

void GUI_SettingsLocation::hideBrowseButton ()
{
	browseButton.setVisible ( false );
}
//-----------------------------------------------------------------------------

void GUI_SettingsLocation::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/components/settings-location.json" } );
}
//-----------------------------------------------------------------------------

void GUI_SettingsLocation::updateStatus ( GUI_SettingsLocationStatus::Status _status, const juce::String& message )
{
	// The stored path can differ from what was picked, an HVSC root resolves to its
	// C64Music folder
	path.setText ( settings->get<juce::String> ( "paths/" + settingName ) );

	status.setStatus ( _status, message );
}
//-----------------------------------------------------------------------------
