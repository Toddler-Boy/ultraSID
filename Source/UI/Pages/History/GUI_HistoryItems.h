#pragma once

#include <JuceHeader.h>

#include "Data/History.h"
#include "UI/Components/GUI_ListBox.h"

class GUI_Pages;

//-----------------------------------------------------------------------------

// The history page's list: a view on the shared History, one row per entry
class GUI_HistoryItems final : public GUI_ListBox, private juce::Timer
{
public:
	GUI_HistoryItems ( GUI_Pages& pages );

	// this

	// Rebuilds the rows from the model
	void reload ();

	void clearAll ();
	void clearOlderThan ( const double days );

	// Re-resolve the cached entry pointers after a user-tune database change;
	// rows whose tune no longer exists are kept, with a null pointer
	void refreshRowData ();

	// GUI_ListBox
	[[ nodiscard ]] juce::String getMissingRowText ( const int rowNumber ) const override;

	// juce::Component
	void visibilityChanged () override;

	// juce::TableListBox
	void paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected ) override;

	// juce::TableListBoxModel
	void cellClicked ( int row, int column, const juce::MouseEvent& ) override;
	void returnKeyPressed ( int lastRowSelected ) override;

private:
	// juce::Timer
	void timerCallback () override;

	GUI_Pages&	pages;

	juce::SharedResourcePointer<History>	history;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_HistoryItems )
};
//-----------------------------------------------------------------------------
