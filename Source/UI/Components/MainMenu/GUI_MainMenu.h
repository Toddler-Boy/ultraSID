#pragma once

#include <JuceHeader.h>

#include "GUI_MainMenuButton.h"

//-----------------------------------------------------------------------------

class GUI_MainMenu final : public juce::Component
{
public:
	GUI_MainMenu ();

	// this
	void updateState ( const juce::String& name );
	void enableMenus ( const bool enable );
	void setExportBadgeCount ( const int count )		{	mbExport.setWorkCount ( count );	}
	void setExportBadgeErrorCount ( const int count )	{	mbExport.setErrorCount ( count );	}

	// Anchor for the +N enqueue badge
	[[ nodiscard ]] juce::Point<int> exportScreenAnchor () const	{	return mbExport.workBadgeScreenAnchor ();	}

private:
	GUI_MainMenuButton	mbSearch	{ "search" };
	GUI_MainMenuButton	mbPlaylist	{ "playlists" };
	GUI_MainMenuButton	mbHistory	{ "history" };
	GUI_MainMenuButton	mbCRT		{ "crt" };
	GUI_MainMenuButton	mbExport	{ "export" };
	GUI_MainMenuButton	mbSettings	{ "settings" };

	std::vector<GUI_MainMenuButton*>	navigation = { &mbSearch, &mbPlaylist, &mbHistory, &mbCRT, &mbExport, &mbSettings };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_MainMenu )
};
//-----------------------------------------------------------------------------
