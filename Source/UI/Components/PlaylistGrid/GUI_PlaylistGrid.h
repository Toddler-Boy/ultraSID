#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_SVG_Button.h"
#include "ultra-shared/UI/Components/GUI_ViewportSmoothScroll.h"

#include "GUI_PlaylistGridItem.h"

class GUI_Pages;

//-----------------------------------------------------------------------------

class GUI_AutoScrollContainer : public juce::Component, public juce::Timer
{
public:
	GUI_AutoScrollContainer ();
	~GUI_AutoScrollContainer () override;

	// juce::Timer
	void timerCallback () override;

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_AutoScrollContainer )
};
//-----------------------------------------------------------------------------

class GUI_PlaylistGrid : public juce::Component
{
public:
	GUI_PlaylistGrid ( GUI_Pages& pages, const bool mini );
	~GUI_PlaylistGrid () override;

	// juce::Component
	void resized () override;

	// this
	void setPlaylists ( const juce::StringArray& list );
	void addPlaylist ( const juce::String& name, const bool withSort = true );
	void removePlaylist ( const juce::String& name );
	void selectPlaylist ( const juce::String& name );

	void updateContent ();
	void updateGridItemByName ( const juce::String& name );

	// The mini grid's "+" button, the keyboard shortcut's way to a new playlist
	void clickAddPlaylist ()		{	headerButton.triggerClick ();	}

private:
	GUI_Pages&	pages;
	const bool	mini;

	void updateGridItem ( GUI_PlaylistGridItem& item );
	void scrollToItem ( const GUI_PlaylistGridItem& item );
	void setCursor ( const int index );
	void itemFocused ( GUI_PlaylistGridItem& item, FocusChangeType cause );
	void layout ();

	// One tab stop: the focused item is the cursor. Arrows, Page Up/Down and
	// Home/End move it, Enter opens it, Tab leaves the grid. The mini list
	// moves up/down only, the grid in all four directions
	bool navigate ( const juce::KeyPress& key );

	class ItemKeys final : public juce::KeyListener
	{
	public:
		ItemKeys ( GUI_PlaylistGrid& _owner ) : owner ( _owner ) {}

		bool keyPressed ( const juce::KeyPress& key, juce::Component* ) override	{	return owner.navigate ( key );	}

	private:
		GUI_PlaylistGrid&	owner;
	};

	ItemKeys	itemKeys { *this };

	GUI_DynamicLabel	header { "playlist/my_playlists", UI::fonts::grid_big_header, UI::colors::textMuted };
	GUI_SVG_Button		headerButton { "addPlaylist", { "playlist/add" } };
	juce::Viewport		viewport;
		GUI_AutoScrollContainer	grid;
	GUI_ViewportSmoothScroll	smoothScroll { viewport };

	juce::OwnedArray<GUI_PlaylistGridItem>	items;

	int	itemWidth = 0;
	int	itemHeight = 0;
	int	itemsPerRow = 1;
	int	cursor = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_PlaylistGrid )
};
//-----------------------------------------------------------------------------
