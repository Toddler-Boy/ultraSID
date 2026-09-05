#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_SearchBar.h"

#include "Data/Tags.h"
#include "UI/Components/GUI_TagButton.h"
#include "UI/ui-colors.h"

#include "GUI_Results.h"

class GUI_Pages;

//-----------------------------------------------------------------------------

class GUI_Search final : public juce::Component
{
public:
	GUI_Search ( GUI_Pages& browser );
	~GUI_Search () override;

	// juce::Component
	void lookAndFeelChanged () override;

	// this
	void setDatabase ( std::vector<const Database::entry*> db );
	void setUserDatabase ( std::vector<const Database::entry*> db );

	int search ( const juce::String& str );
	int updateSearch ();

	// Untoggle the tag and like filter buttons
	void clearFilters ();

	// Empty search with only the like filter on: the liked tunes
	void showLiked ();

	GUI_SearchBar	searchbar;
	GUI_Results		results;

private:
	juce::SharedResourcePointer<Tags>	tags;

	std::vector<GUI_TagButton*>	tagComps;

	GUI_TagButton	liked { "search/tag/liked", UI::colors::tagLiked };
	GUI_Label		info { "", UI::fonts::search_info };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Search )
};
//-----------------------------------------------------------------------------
