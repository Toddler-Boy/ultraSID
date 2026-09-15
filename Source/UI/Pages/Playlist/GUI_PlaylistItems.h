#pragma once

#include <JuceHeader.h>

#include "Data/Playlists.h"
#include "UI/Components/GUI_ListBox.h"

class GUI_Pages;

//-----------------------------------------------------------------------------

class GUI_PlaylistItems final : public GUI_ListBox, public juce::DragAndDropTarget
{
public:
	GUI_PlaylistItems ( GUI_Pages& browser, const juce::String& name );

	// juce::Component
	void setName ( const juce::String& newName ) override;

	// juce::TableListBoxModel
	void returnKeyPressed ( int lastRowSelected ) override;
	void cellClicked ( int row, int columnId, const juce::MouseEvent& e ) override;
	juce::var getDragSourceDescription ( const juce::SparseSet<int>& rowsToDescribe ) override;
	void sortOrderChanged ( int newSortColumnId, bool isForwards ) override;

	// juce::ListBox
	void paintOverChildren ( juce::Graphics& g ) override;

	// GUI_ListBox
	void paintRowBackground ( juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected ) override;
	[[ nodiscard ]] juce::String getMissingRowText ( const int rowNumber ) const override;

	// The number column shows the custom-order position
	[[ nodiscard ]] int getRowNumber ( const int rowNumber ) const override	{	return realPlaylist->getEntryIndex ( rowNumber ) + 1;	}

	// this
	[[ nodiscard ]] int getShuffled ( const int position ) const	{	return realPlaylist->getShuffled ( position );	}
	void createShuffle ()	{ realPlaylist->createShuffle (); }
	void updateRowData ();

	void clear ();
	[[ nodiscard ]] juce::Image getCoverImage () const { return realPlaylist->getCoverImage (); }

	[[ nodiscard ]] const Database::entry* getItem ( const int index ) const	{	return rowData[ index ]; }
	[[ nodiscard ]] int16_t getSubtune ( const int index ) const				{	return rowSubtune[ index ]; }
	[[ nodiscard ]] int getSize () const { return int ( rowData.size () ); }

	[[ nodiscard ]] const std::vector<const Database::entry*>& getEntries ()	{	return rowData;	}

	// juce::DragAndDropTarget
	bool isInterestedInDragSource ( const SourceDetails& dragSourceDetails ) override;
	void itemDropped ( const SourceDetails& dragSourceDetails ) override;
	void itemDragEnter ( const SourceDetails& dragSourceDetails ) override;
	void itemDragMove ( const SourceDetails& dragSourceDetails ) override;
	void itemDragExit ( const SourceDetails& dragSourceDetails ) override;

private:
	// The header arrow follows the playlist's own sort state
	void syncHeaderSort ();

	// Insert row under the drag, -1 = append
	[[ nodiscard ]] int getDropRow ( const SourceDetails& dragSourceDetails ) const;

	juce::SharedResourcePointer<Playlists>	playlists;

	playlist*	realPlaylist = nullptr;

	GUI_Pages&	browser;

	int dragOverRow = -1;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_PlaylistItems )
};
//-----------------------------------------------------------------------------
