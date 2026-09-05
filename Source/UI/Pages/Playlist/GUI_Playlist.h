#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_SVG_Button.h"

#include "Data/Playlists.h"
#include "UI/Components/GUI_CoverDisplay.h"
#include "UI/Components/GUI_MenuButton.h"
#include "UI/Components/GUI_PlayButton.h"

#include "GUI_PlaylistItems.h"

//-----------------------------------------------------------------------------

class GUI_Pages;

class GUI_Playlist final
	: public juce::Component
	, public juce::ChangeListener
	, public juce::FileDragAndDropTarget
	, public juce::TextDragAndDropTarget
{
public:
	GUI_Playlist ( GUI_Pages& pages );
	~GUI_Playlist () override;

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;
	void lookAndFeelChanged () override;

	// juce::ChangeListener
	void changeListenerCallback ( juce::ChangeBroadcaster* source ) override;

	// juce::FileDragAndDropTarget
	bool isInterestedInFileDrag ( const juce::StringArray& files ) override;
	void filesDropped ( const juce::StringArray& files, int x, int y ) override;

	// juce::TextDragAndDropTarget
	bool isInterestedInTextDrag ( const juce::String& text ) override;
	void textDropped ( const juce::String& text, int x, int y ) override;

	// this
	void setPlaylists ( const juce::StringArray& list );
	void updatePlaylist ( const juce::String& name );
	void addPlaylist ( const juce::String& filename );
	void renamePlaylist ( const std::string& oldName, const std::string& newName );
	void deletePlaylist ( const juce::String& name );
	void showPlaylist ( const juce::String& name );
	void updateInfo ();

	// Rebuild every open playlist view's entry pointers from its playlist's
	// tune keys, after a user-tune database change freed/replaced entries
	void refreshRowData ();

	[[ nodiscard ]] GUI_PlaylistItems* getPlaylistItems ( const juce::String& name );

	GUI_PlaylistItems*	currentVisible = nullptr;

private:
	// this
	void showMenu ();

	juce::SharedResourcePointer<Playlists>	playlists;

	GUI_Pages&			pages;

	GUI_SVG_Button		backButton { "back", { "history_back" } };
	GUI_MenuButton		menuButton { "playlist" };
	GUI_CoverDisplay	coverDisplay;
	GUI_PlayButton		playButton;
	juce::Label			header { "header" };
	GUI_Label			info { "", UI::fonts::playlist_info };
	juce::Image			blurredBackground;
	juce::Image			noiseImage;

	juce::Rectangle<int>	listBounds;

	std::unordered_map<std::string, std::unique_ptr<GUI_PlaylistItems>>	plMap;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Playlist )
};
//-----------------------------------------------------------------------------
