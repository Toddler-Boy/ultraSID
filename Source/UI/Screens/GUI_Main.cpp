#include "GUI_Main.h"

#include "ultra-shared/UI/UI_Helpers.h"

GUI_Main::GUI_Main ( juce::AudioDeviceManager& deviceManager )
	: juce::Component ( "main" )
	, pages ( deviceManager )
	, sidebarLeft ( pages )
{
	// Sidebar left (main menu and mini-playlists)
	sidebarLeft.addAndMakeVisible ( badge );
	addAndMakeVisible ( sidebarLeft );

	// Sidebar right (STIL and visualizations)
	addAndMakeVisible ( sidebarRight );

	// Footer with transport, volume control, etc.
	addAndMakeVisible ( footer );

	// Pages (search, playlist, history, crt settings, etc.)
	addAndMakeVisible ( pages );

	// Last, so it overlays everything above
	addChildComponent ( alertBanner );
}
//-----------------------------------------------------------------------------

void GUI_Main::resized ()
{
	const static juce::StringArray	sidebarForbidden { "settings", "crt" };

	layout.setConstant ( "sidebarRightAllowed", sidebarForbidden.contains ( pages.getPage () ) ? 0 : 1 );

	UI::setLayout ( layout, {	"UI/layouts/constants.json",
							"UI/layouts/screens/main.json" } );
}
//-----------------------------------------------------------------------------
