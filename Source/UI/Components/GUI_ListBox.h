#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_ListBoxMouseMoveHover.h"
#include "ultra-shared/UI/Components/GUI_ViewportSmoothScroll.h"

#include "Data/History.h"
#include "Database/Database.h"
#include "Database/HVSCDatabase.h"

class Icons;
class Strings;
class Tags;
class ThumbnailCache;
class Likes;

//-----------------------------------------------------------------------------

class GUI_ListBox : public juce::TableListBox, public juce::TableListBoxModel, public juce::ChangeListener, private juce::ComponentMovementWatcher
{
public:
	enum columnId : int8_t
	{
		number = 1,	// juce::TableHeaderComponent forbids a columnId of 0
		animation,
		name,
		release,
		information,
		length,

		liked,

		// History
		historyDate,

		// Export
		exportProgress,
	};

	GUI_ListBox ();
	~GUI_ListBox () override;

	// juce::ListBox
	bool keyPressed ( const juce::KeyPress& key ) override;

	// juce::Component
	void paintOverChildren ( juce::Graphics& g ) override;

	// juce::TableListBox
	void resized () override;
	void paintRowBackground ( juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected ) override;
	void paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected ) override;

	// juce::TableListBoxModel
	void cellClicked ( int rowNumber, int columnId, const juce::MouseEvent& e ) override;
	void cellDoubleClicked ( int rowNumber, int columnId, const juce::MouseEvent& e ) override;
	juce::String getCellTooltip ( int rowNumber, int columnId ) override;

	int getNumRows () override;
	void sortOrderChanged ( int newSortColumnId, bool isForwards ) override;
	juce::var getDragSourceDescription ( const juce::SparseSet<int>& rowsToDescribe ) override;

	// this
	void addHeaderColumn ( const int colId, bool sortable = false );

	void timerUpdate ( const float secondsPassed );
	void setPlayingName ( const std::string& tuneName );
	void setPlayingRow ( const int rowNumber );
	[[ nodiscard ]] int getPlayingRow () const	{	return rowPlaying;	}
	[[ nodiscard ]] const Database::entry* getRow ( const int rowNumber );

	// Select the row and scroll it into view; -1 is a no-op
	void showRow ( const int rowNumber );

	// First row holding the tune, a subtune match wins; -1 when absent
	[[ nodiscard ]] int findRow ( const std::string& lowerFile, const int subtune ) const;

	// juce::ChangeListener
	void changeListenerCallback ( juce::ChangeBroadcaster* source ) override;
	int		hoverPosition = -1;
	GUI_ListBoxMouseMoveHover	hover;
	GUI_ViewportSmoothScroll	smoothScroll;

protected:
	static constexpr auto	fontSize = 17.0f;

	// The exports list keeps the history's retention policy
	static constexpr auto	maxRetainedItems = History::maxRetainedItems;
	static constexpr auto	maxRetainedAgeDays = History::maxRetainedAgeDays;

	juce::SharedResourcePointer<Icons>			icons;
	juce::SharedResourcePointer<Strings>		strings;
	juce::SharedResourcePointer<Tags>			tags;
	juce::SharedResourcePointer<Likes>			likes;
	juce::SharedResourcePointer<ThumbnailCache>	thumbnailCache;
	juce::SharedResourcePointer<HVSC_database>	hvscDB;

	[[ nodiscard ]] int getRealSubtune ( const int rowNumber ) const;
	[[ nodiscard ]] juce::StringArray getTuneList ( const juce::SparseSet<int>& rows, const bool withSubtunes = true ) const;
	[[ nodiscard ]] juce::String getTuneFolder ( const juce::SparseSet<int>& rows ) const;

	// What the name column shows for a row whose tune the database no longer
	// resolves (null rowData entry)
	[[ nodiscard ]] virtual juce::String getMissingRowText ( const int /*rowNumber*/ ) const	{	return {};	}

	// juce::ComponentMovementWatcher, watching this list itself: coming on
	// screen takes the keyboard focus and defaults to the first row selected
	void componentMovedOrResized ( bool /*wasMoved*/, bool /*wasResized*/ ) override	{}
	void componentPeerChanged () override	{}
	void componentVisibilityChanged () override;

	bool			filterExactMatch = true;

	// Strings key announced centered in the viewport while the list is empty;
	// an empty key shows nothing
	juce::String	placeholderKey;

	bool			useNameOnly = false;
	int				rowPlaying = -1;
	std::string		tunePlaying;

	float			animSpeed = 0.0f;

	std::vector<const Database::entry*>		rowData;
	std::vector<int16_t>					rowSubtune;
};
//-----------------------------------------------------------------------------
