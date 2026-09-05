#include <JuceHeader.h>

#include "GUI_History.h"

#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/UI_Menus.h"

#include "../GUI_Pages.h"

//-----------------------------------------------------------------------------

GUI_History::GUI_History ( GUI_Pages& _browser )
	: historyItems ( _browser )
{
	setName ( "history" );

	label.setName ( "header" );

	menuButton.onClick = [ this ] { showMenu (); };

	addAndMakeVisible ( label );
	addAndMakeVisible ( menuButton );
	addAndMakeVisible ( historyItems );
}
//-----------------------------------------------------------------------------

void GUI_History::visibilityChanged ()
{
	historyItems.visibilityChanged ();
}
//-----------------------------------------------------------------------------

void GUI_History::addItem ( const std::string& tuneName, int subtune )
{
	juce::SharedResourcePointer<History> ()->add ( tuneName, subtune );
	historyItems.reload ();
}
//-----------------------------------------------------------------------------

void GUI_History::showMenu ()
{
	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	auto	m = UI::newPopupMenu ( *this );

	UI::menu_ClearOlderThan ( m, [ this ] ( double days ) { historyItems.clearOlderThan ( days ); } );

	// Clear history
	m.addSeparator ();

	m.addItem ( UI::newDangerousMenuItem ( strings->get ( "menu/clear_history" ), icons->get ( "menu/delete" ), [ this ]
	{
		historyItems.clearAll ();
	} ) );

	UI::showMenuAtButton ( m, *this, menuButton );
}
//-----------------------------------------------------------------------------
