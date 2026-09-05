#pragma once

#include <JuceHeader.h>

#include <vector>

#include "ProcessingThread.h"

//-----------------------------------------------------------------------------

// Table model for the processing-queue views. Reads straight from the processing
// thread's queue, or (when given an entry list) from that snapshot instead
// (the active-only view, or the full queue filtered down to errors)

class QueueModel final : public juce::TableListBoxModel
{
public:
	enum ColumnIds
	{
		fileColumn = 1,
		statusColumn,
		lengthColumn,
		progressColumn
	};

	explicit QueueModel ( const ProcessingThread& threadIn, const std::vector<const ProcessingThread::QueueEntry*>* entryListIn = nullptr );

	// Registers this model with the table and sets up its columns
	void attachTo ( juce::TableListBox& tableIn );

	// Switches between the full queue (nullptr) and a snapshot list
	void setEntryList ( const std::vector<const ProcessingThread::QueueEntry*>* entryListIn );

	int getNumRows () override;
	void paintRowBackground ( juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected ) override;
	void paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected ) override;

private:
	const ProcessingThread::QueueEntry* getEntry ( int rowNumber ) const;
	juce::LookAndFeel& getLookAndFeel () const;

	const ProcessingThread&	processingThread;
	const std::vector<const ProcessingThread::QueueEntry*>*	entryList;
	juce::TableListBox*		table = nullptr;

	// Rounded outline of a progress cell's bar, used as a clip while painting;
	// rebuilt on the fly whenever the cell size changes
	juce::Path				progressBarPath;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( QueueModel )
};
//-----------------------------------------------------------------------------
