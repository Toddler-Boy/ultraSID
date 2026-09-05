#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"

#include "Config/Preferences.h"
#include "UI/Components/GUI_MenuButton.h"

#include "GUI_ExportItems.h"


//-----------------------------------------------------------------------------

class GUI_Export final : public juce::Component
{
public:
	GUI_Export ();

	// this
	void update ();
	void load () { exportItems.load (); }
	void refreshRowData () { exportItems.refreshRowData (); }
	void repaintCell ( const int index ) { exportItems.repaintProgressCell ( index ); }

	void addItem ( const std::string& filename, const std::string& tuneName );

	void showTune ( const std::string& lowerFile, const int subtune )	{	exportItems.showRow ( exportItems.findRow ( lowerFile, subtune ) );	}

	// The footer quality's FX mode (SIDEffects::FXMode), the mode exports
	// render with
	[[ nodiscard ]] static int fxModeForQuality ( const juce::String& quality );

private:
	// this
	void showMenu ();

	juce::SharedResourcePointer<Preferences>	preferences;

	juce::CriticalSection	exportLock;

	GUI_DynamicLabel	label { "export/header", UI::fonts::page_title };
	GUI_MenuButton		menuButton { "export" };
	GUI_ExportItems	exportItems;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Export )
};
//-----------------------------------------------------------------------------
