#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"

#include "UI/Components/GUI_MenuButton.h"

#include "GUI_HistoryItems.h"


//-----------------------------------------------------------------------------

class GUI_Pages;

class GUI_History final : public juce::Component
{
public:
	GUI_History ( GUI_Pages& browser );

	// juce::Component
	void visibilityChanged () override;

	// this
	void load () { historyItems.reload (); }
	void refreshRowData () { historyItems.refreshRowData (); }

	void addItem ( const std::string& tuneName, int subtune );

	void showTune ( const std::string& lowerFile, const int subtune )	{	historyItems.showRow ( historyItems.findRow ( lowerFile, subtune ) );	}

private:
	// this
	void showMenu ();

	GUI_DynamicLabel	label { "history/header", UI::fonts::page_title };
	GUI_MenuButton		menuButton { "history" };
	GUI_HistoryItems	historyItems;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_History )
};
//-----------------------------------------------------------------------------
