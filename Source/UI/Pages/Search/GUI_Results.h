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

	// The last search found nothing as typed and the rows are one typo off
	[[ nodiscard ]] bool isCloseMatch () const		{	return closeMatch;	}

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

	// GUI_ListBox
	[[ nodiscard ]] std::vector<db::textSpan> getHighlights ( const std::string_view shown, const textField field ) const override;

private:
	GUI_Pages&	pages;

	// A word matches any search field unless a prefix limits it to one
	struct searchTerm
	{
		std::string_view Database::entry::*	field;		// null: any field
		std::string							text;
	};

	// The terms of the last search, for the highlights
	std::vector<searchTerm>	terms;
	bool					fuzzy = false;

	std::vector<const Database::entry*>	database;
	std::vector<const Database::entry*>	userDatabase;

	// The results in search order, restored when the column sort clears
	std::vector<const Database::entry*>	unsorted;

	juce::String		searchPattern;
	searchOptions		searchOpts;
	int					searchScreenshots = -1;		// ScreenshotLookup generation the results are from
	bool				closeMatch = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Results )
};
//-----------------------------------------------------------------------------
