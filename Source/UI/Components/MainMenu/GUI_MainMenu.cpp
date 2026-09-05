#include <JuceHeader.h>

#include "GUI_MainMenu.h"

#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

GUI_MainMenu::GUI_MainMenu ()
{
	setName ( "mainMenu" );

	for ( auto nav : navigation )
	{
		addAndMakeVisible ( nav );

		nav->onClick = [ nav ]
		{
			if ( nav->getToggleState () )
				return;

			nav->setToggleState ( true, juce::dontSendNotification );

			msg::MainMenu { nav->getName () }.send ();
		};
	}
}
//-----------------------------------------------------------------------------

void GUI_MainMenu::updateState ( const juce::String& name )
{
	for ( auto nav : navigation )
		nav->setToggleState ( nav->getName () == name, juce::dontSendNotification );
}
//-----------------------------------------------------------------------------

void GUI_MainMenu::enableMenus ( const bool enable )
{
	for ( auto nav : navigation )
		nav->setEnabled ( enable );

	mbSettings.setEnabled ( true );

	if ( ! enable )
		updateState ( mbSettings.getName () );
}
//-----------------------------------------------------------------------------
