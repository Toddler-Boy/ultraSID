#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Line.h"

#include "Config/Preferences.h"
#include "Resources/STIL_Lookup.h"
#include "UI/Components/GUI_TagButton.h"
#include "UI/ui-colors.h"

#include "GUI_STILHelper.h"
#include "List/GUI_STIL_ListView.h"
#include "Text/GUI_STIL_TextView.h"

//-----------------------------------------------------------------------------

// STIL's toggle buttons: the tag-button behavior, themed through their own
// role set
class GUI_STILToggle final : public GUI_TagButton
{
public:
	GUI_STILToggle ( const juce::String& name, const int colorId )
		: GUI_TagButton ( name, colorId, {	UI::fonts::stil_toggle,
											UI::corners::stil_toggle,
											UI::lines::stil_toggle,
											UI::lines::stil_toggle_on } )
	{
	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STILToggle )
};
//-----------------------------------------------------------------------------

class GUI_STILView final : public juce::Component, public GUI_STILHelper
{
public:
	GUI_STILView ();

	// this
	void setSTIL_blocks ( const juce::String& filename, const GUI_STIL_blocks& blocks );
	void restorePreferences ();

	void setTune ( const juce::String& filename, const int mainTuneNo );
	void setTuneLength ( const int tune, const uint32_t lengthMS );
	void setTunePlaying ( const int tune );
	void setDefaultTune ( const std::string& title, const int tune );
	void updateFilterStates ();

	void timerUpdate ( const float secondsPassed );

	// Inputs for the sidebar's layout: it positions this view's children and
	// decides whether the visualizations make room
	[[ nodiscard ]] bool showInformation () const
	{
		return preferences->get<bool> ( "stil/show-information" ) && view.hasInformation ();
	}

	[[ nodiscard ]] int textNeededHeight () const	{	return showInformation () ? view.contentHeight () : 0;	}

	// The actual text space of the current layout, valid after a layout pass
	[[ nodiscard ]] int textViewHeight () const	{	return view.isVisible () ? view.getHeight () : 0;	}

	[[ nodiscard ]] int listMaxHeight ()	{	return list.getMaximumHeight ();	}
	[[ nodiscard ]] bool vizAlways () const		{	return preferences->get<bool> ( "stil/viz-always" );	}

	// The pin only matters while the chips would actually make room
	void setVizToggleEnabled ( const bool enabled )	{	toggleVizAlways.setEnabled ( enabled );	}

	// GUI_STILHelper
	juce::Image getAuthorImage ( const juce::String& authorPath ) override;
	juce::Image getBugsImage ( const juce::String& author ) override;

private:
	juce::SharedResourcePointer<Preferences>	preferences;


	GUI_STILToggle	toggleTunesOnly { "stil/toggle/tunes_only", UI::colors::stilToggleTunesOnly };
	GUI_STILToggle	toggleInformation { "stil/toggle/stil", UI::colors::stilToggleSTIL };
	GUI_STILToggle	toggleVizAlways { "stil/toggle/viz", UI::colors::stilToggleViz };

	GUI_STIL_ListView	list;
	GUI_Line			line { "line" };
	GUI_STIL_TextView	view;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STILView )
};
//-----------------------------------------------------------------------------
