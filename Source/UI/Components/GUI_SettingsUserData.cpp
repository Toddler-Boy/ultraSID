#include "GUI_SettingsUserData.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "Helpers/Messages.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_SettingsUserData::GUI_SettingsUserData ()
	: juce::Component ( "user-data" )
	, label ( "settings/user-data/title", UI::fonts::settings_entry, UI::colors::text )
	, help ( "settings/user-data/help", UI::fonts::settings_help, UI::colors::textMuted )
{
	help.setName ( "help" );

	addAndMakeVisible ( label );
	addAndMakeVisible ( help );

	for ( auto i = 0; i < int ( rows.size () ); ++i )
	{
		const auto	id = userdata::idOf ( userdata::Category ( i ) );

		rows[ i ].toggle = std::make_unique<GUI_Checkbox> ( id );
		rows[ i ].toggle->setToggleState ( true, juce::dontSendNotification );
		rows[ i ].label = std::make_unique<GUI_Label> ( "", UI::fonts::settings_location, UI::colors::text );
		rows[ i ].label->setName ( id + "-label" );

		addAndMakeVisible ( *rows[ i ].toggle );
		addAndMakeVisible ( *rows[ i ].label );
	}

	addAndMakeVisible ( exportButton );
	addAndMakeVisible ( importButton );
	addChildComponent ( status );

	exportButton.onClick = [ this ]	{	exportArchive ();	};
	importButton.onClick = [ this ]	{	importArchive ();	};

	refresh ();
}
//-----------------------------------------------------------------------------

void GUI_SettingsUserData::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/components/settings-user-data.json" } );
}
//-----------------------------------------------------------------------------

void GUI_SettingsUserData::refresh ()
{
	const auto	root = userRoot ();
	auto		anyContent = false;

	for ( auto i = 0; i < int ( rows.size () ); ++i )
	{
		const auto	category = userdata::Category ( i );
		const auto	numFiles = userdata::listFiles ( root, category ).size ();
		auto&		row = rows[ i ];

		// "(3 files)", "(1 entry)", "(empty)"; the lists report their loaded
		// entries, preferences carry no count worth showing
		const auto	counted = [ this ] ( const char* one, const char* many, const int n )
		{
			return strings->get ( juce::String ( "settings/user-data/" ) + ( n == 1 ? one : many ) ).replace ( "{}", juce::String ( n ) );
		};

		juce::String	count;

		if ( numFiles == 0 )
			count = strings->get ( "settings/user-data/none" );
		else if ( category == userdata::Category::likes )
			count = counted ( "entry", "entries", likes->numEntries () );
		else if ( category == userdata::Category::history )
			count = counted ( "entry", "entries", history->numEntries () );
		else if ( category != userdata::Category::preferences )
			count = counted ( "file", "files", numFiles );

		row.label->setText ( strings->get ( "settings/user-data/" + userdata::idOf ( category ) ) + ( count.isEmpty () ? "" : " (" + count + ")" ) );

		// An empty category has nothing to pick; one that just gained content
		// joins the selection
		if ( numFiles == 0 )
			row.toggle->setToggleState ( false, juce::dontSendNotification );
		else if ( ! row.toggle->isEnabled () )
			row.toggle->setToggleState ( true, juce::dontSendNotification );

		row.toggle->setEnabled ( numFiles > 0 );
		anyContent = anyContent || numFiles > 0;
	}

	exportButton.setEnabled ( anyContent );
	importButton.setEnabled ( root.isDirectory () );
}
//-----------------------------------------------------------------------------

juce::File GUI_SettingsUserData::userRoot () const
{
	const auto	path = settings->get<juce::String> ( "paths/user" );

	return path.isEmpty () ? juce::File () : juce::File ( path );
}
//-----------------------------------------------------------------------------

std::vector<userdata::Category> GUI_SettingsUserData::checkedCategories () const
{
	std::vector<userdata::Category>	picked;

	for ( auto i = 0; i < int ( rows.size () ); ++i )
		if ( rows[ i ].toggle->isEnabled () && rows[ i ].toggle->getToggleState () )
			picked.push_back ( userdata::Category ( i ) );

	return picked;
}
//-----------------------------------------------------------------------------

void GUI_SettingsUserData::setStatus ( const GUI_SettingsLocationStatus::Status _status, const juce::String& message )
{
	status.setStatus ( _status, message );
}
//-----------------------------------------------------------------------------

void GUI_SettingsUserData::exportArchive ()
{
	const auto	picked = checkedCategories ();
	if ( picked.empty () )
	{
		setStatus ( GUI_SettingsLocationStatus::warning, strings->get ( "settings/user-data/nothing-checked" ) );
		return;
	}

	const auto	defaultFile = juce::File::getSpecialLocation ( juce::File::userDocumentsDirectory )
								.getChildFile ( juce::String ( ProjectInfo::projectName ) + " user-data " + juce::Time::getCurrentTime ().formatted ( "%Y-%m-%d" ) + ".zip" );

	auto	chooser = std::make_shared<juce::FileChooser> ( strings->get ( "settings/user-data/export-title" ), defaultFile, "*.zip" );

	constexpr auto	chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
	chooser->launchAsync ( chooserFlags, [ this, chooser, picked ] ( const juce::FileChooser& )
	{
		auto	dst = chooser->getResult ();
		if ( dst == juce::File () )
			return;

		if ( ! dst.hasFileExtension ( "zip" ) )
			dst = dst.withFileExtension ( "zip" );

		const auto	numFiles = userdata::exportArchive ( userRoot (), picked, dst );

		if ( numFiles < 0 )
			setStatus ( GUI_SettingsLocationStatus::error, strings->get ( "settings/user-data/export-failed" ) );
		else
			setStatus ( GUI_SettingsLocationStatus::ok, strings->get ( "settings/user-data/exported" ).replace ( "{}", juce::String ( numFiles ) ) );
	} );
}
//-----------------------------------------------------------------------------

void GUI_SettingsUserData::importArchive ()
{
	auto	chooser = std::make_shared<juce::FileChooser> ( strings->get ( "settings/user-data/import-title" ), juce::File::getSpecialLocation ( juce::File::userDocumentsDirectory ), "*.zip" );

	constexpr auto	chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
	chooser->launchAsync ( chooserFlags, [ this, chooser ] ( const juce::FileChooser& )
	{
		const auto	src = chooser->getResult ();
		if ( ! src.existsAsFile () )
			return;

		const auto	found = userdata::inspectArchive ( src );
		if ( ! found )
		{
			setStatus ( GUI_SettingsLocationStatus::error, strings->get ( "settings/user-data/not-archive" ) );
			return;
		}

		// Only the checked categories the archive actually holds
		std::vector<userdata::Category>	picked;
		juce::StringArray				names;

		for ( const auto category : checkedCategories () )
		{
			if ( std::ranges::find ( *found, category ) == found->end () )
				continue;

			picked.push_back ( category );
			names.add ( strings->get ( "settings/user-data/" + userdata::idOf ( category ) ) );
		}

		if ( picked.empty () )
		{
			setStatus ( GUI_SettingsLocationStatus::warning, strings->get ( "settings/user-data/nothing-selected" ) );
			return;
		}

		const auto	options = juce::MessageBoxOptions ()
								.withIconType ( juce::MessageBoxIconType::QuestionIcon )
								.withTitle ( strings->get ( "settings/user-data/import-title" ) )
								.withMessage ( strings->get ( "settings/user-data/import-message" ).replace ( "{}", names.joinIntoString ( ", " ) ) )
								.withButton ( strings->get ( "settings/user-data/merge" ) )
								.withButton ( strings->get ( "settings/user-data/replace" ) )
								.withButton ( strings->get ( "settings/user-data/cancel" ) )
								.withAssociatedComponent ( this );

		// Buttons report as 1, 2 and 0 in order
		juce::NativeMessageBox::showAsync ( options, [ safe = juce::Component::SafePointer<GUI_SettingsUserData> ( this ), src, picked ] ( int result )
		{
			if ( result == 0 || safe == nullptr )
				return;

			// Pending preference edits would otherwise be flushed over the
			// imported file when the folder reloads
			safe->preferences->save ();

			const auto	mode = result == 1 ? userdata::Mode::merge : userdata::Mode::replace;
			const auto	numFiles = userdata::importArchive ( safe->userRoot (), src, picked, mode );

			if ( numFiles < 0 )
				safe->setStatus ( GUI_SettingsLocationStatus::error, safe->strings->get ( "settings/user-data/import-failed" ) );
			else
				safe->setStatus ( GUI_SettingsLocationStatus::ok, safe->strings->get ( "settings/user-data/imported" ).replace ( "{}", juce::String ( numFiles ) ) );

			// Reloads everything from the folder, this component included
			msg::SetLocation { "user" }.send ();
		} );
	} );
}
//-----------------------------------------------------------------------------
