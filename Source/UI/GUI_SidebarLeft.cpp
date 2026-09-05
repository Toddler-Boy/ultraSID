#include <JuceHeader.h>

#include "GUI_SidebarLeft.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/Pages/GUI_Pages.h"

//-----------------------------------------------------------------------------

GUI_SidebarLeft::GUI_SidebarLeft ( GUI_Pages& pages )
	: juce::Component ( "sidebarLeft" )
	, miniPlaylists ( pages, true )
{
	// Main menu
	addAndMakeVisible ( mainMenu );

	// Mini playlists
	addAndMakeVisible ( miniPlaylists );

	addAndMakeVisible ( rightBorder );
}
//-----------------------------------------------------------------------------

void GUI_SidebarLeft::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/sidebar-left.json"
					   } );
}
//-----------------------------------------------------------------------------
