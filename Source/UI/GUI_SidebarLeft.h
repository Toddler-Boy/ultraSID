#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Line.h"

#include "UI/Components/MainMenu/GUI_MainMenu.h"
#include "UI/Components/PlaylistGrid/GUI_PlaylistGrid.h"

//-----------------------------------------------------------------------------

class GUI_Pages;

// The left sidebar: main menu on top, mini playlist grid below. The widgets
// are private, the app talks to the sidebar through the methods below.

class GUI_SidebarLeft final : public juce::Component
{
public:
	GUI_SidebarLeft ( GUI_Pages& pages );

	// juce::Component
	void resized () override;

	// Main menu
	void enableMenus ( const bool enable )				{	mainMenu.enableMenus ( enable );	}
	void updateMenuState ( const juce::String& name )	{	mainMenu.updateState ( name );	}

	void setExportBadgeCounts ( const int workCount, const int errorCount )
	{
		mainMenu.setExportBadgeCount ( workCount );
		mainMenu.setExportBadgeErrorCount ( errorCount );
	}

	[[ nodiscard ]] juce::Point<int> exportScreenAnchor () const	{	return mainMenu.exportScreenAnchor ();	}

	// Mini playlists
	void setPlaylists ( const juce::StringArray& names )	{	miniPlaylists.setPlaylists ( names );	}
	void addPlaylist ( const juce::String& name )			{	miniPlaylists.addPlaylist ( name );	}
	void removePlaylist ( const juce::String& name )		{	miniPlaylists.removePlaylist ( name );	}
	void selectPlaylist ( const juce::String& name )		{	miniPlaylists.selectPlaylist ( name );	}
	void updatePlaylistItem ( const juce::String& name )	{	miniPlaylists.updateGridItemByName ( name );	}
	void refreshPlaylists ()								{	miniPlaylists.updateContent ();	}
	void clickAddPlaylist ()								{	miniPlaylists.clickAddPlaylist ();	}

private:
	gin::LayoutSupport	layout { *this };

	GUI_MainMenu		mainMenu;
	GUI_PlaylistGrid	miniPlaylists;

	GUI_Line			rightBorder { "rightBorder" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SidebarLeft )
};
//-----------------------------------------------------------------------------
