#pragma once

#include <JuceHeader.h>

#include "App/TuneExporter.h"
#include "UI/Components/GUI_ListBox.h"

//-----------------------------------------------------------------------------

class GUI_ExportItems final : public GUI_ListBox
{
public:
	GUI_ExportItems ();

	// juce::TableListBoxModel
	void cellClicked ( int row, int columnId, const juce::MouseEvent& e ) override;
	void returnKeyPressed ( int lastRowSelected ) override;

	// juce::TableListBox
	void paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected ) override;

	// this
	void clear ();
	void load ();
	void save ();
	void addItem ( const std::string& tuneName, const TuneExporter::entry& queueEntry );
	void update ();

	void clearAll ();
	void clearOlderThan ( const double days );

	void reAddCanceled ();
	[[ nodiscard ]] bool hasCanceled () const;

	// Re-resolve the cached entry pointers after a user-tune database change.
	// Rows stay (lockstep with the exporter queue), unresolvable ones go red
	void refreshRowData ();

	// An entry reached a non-stage status (msg::ExportEntryStatusUpdate),
	// repaint just that row's progress cell
	void repaintProgressCell ( const int index );

	// GUI_ListBox
	[[ nodiscard ]] juce::String getMissingRowText ( const int rowNumber ) const override;

private:
	// Rows display newest-first while the exporter queue runs oldest-first;
	// a row and its queue entry always map through this (its own inverse)
	[[ nodiscard ]] int toQueueIndex ( const int row )	{	return getNumRows () - 1 - row;	}

	// Row lists and the exporter queue stay in lockstep: rows appear/disappear
	// only together with their queue entry
	bool removeEntry ( const int row );
	[[ nodiscard ]] bool addNewEntry ( const std::string& tune, const int tuneNo );

	void reAddRow ( const int row );

	void applyRetention ();

	std::vector<float>	renderProgress;

	// The rows' tune keys, rowData pointers are just a cache resolved from these
	std::vector<std::string>	rowFile;

	juce::SharedResourcePointer<TuneExporter>	tuneExporter;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ExportItems )
};
//-----------------------------------------------------------------------------
