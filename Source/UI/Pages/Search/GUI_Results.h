#pragma once

#include <JuceHeader.h>

#include "UI/Components/GUI_ListBox.h"

class GUI_Pages;

//-----------------------------------------------------------------------------

class GUI_Results final : public GUI_ListBox
{
public:
	GUI_Results ( GUI_Pages& pages );

	// this
	void setDatabase ( std::vector<const Database::entry*> db );
	void setUserDatabase ( std::vector<const Database::entry*> db );

	struct searchOptions
	{
		bool	mustBeLiked;
		bool	mustBePioneer;
		bool	mustBeWinner;
		bool	mustBeGem;
	};

	int search ( const juce::String& str, const searchOptions options );

	// Which filters still match something in the current results; the search
	// page disables dead-end filter buttons from this
	struct filterAvailability
	{
		bool	liked;
		bool	pioneer;
		bool	winner;
		bool	gem;
	};

	[[ nodiscard ]] filterAvailability getFilterAvailability () const;

	// juce::TableListBoxModel
	void cellClicked ( int row, int columnId, const juce::MouseEvent& e ) override;
	void returnKeyPressed ( int lastRowSelected ) override;
	void sortOrderChanged ( int newSortColumnId, bool isForwards ) override;

private:
	GUI_Pages&	pages;

	std::vector<const Database::entry*>	database;
	std::vector<const Database::entry*>	userDatabase;

	// The results in search order, restored when the column sort clears
	std::vector<const Database::entry*>	unsorted;

	juce::String		searchPattern;
	searchOptions		searchOpts;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Results )
};
//-----------------------------------------------------------------------------
