#include <JuceHeader.h>

#include "GUI_STILView.h"

#include "Resources/STIL_Lookup.h"

//-----------------------------------------------------------------------------

GUI_STILView::GUI_STILView ()
{
	setName ( "stil" );

	auto addToggle = [ this ] ( GUI_TagButton& but, const juce::String& setName )
	{
		but.onClick = [ this, &but, setName ]
		{
			preferences->set ( "stil/" + setName, but.getToggleState () );
			list.layout ();

			// The sidebar lays out this view's children and re-balances STIL
			// vs visualizations from these states
			if ( auto parent = getParentComponent () )
				parent->resized ();
		};

		addAndMakeVisible ( but );
	};

	addToggle ( toggleTunesOnly, "show-tunes-only" );
	addToggle ( toggleInformation, "show-information" );
	addToggle ( toggleVizAlways, "viz-always" );

	addAndMakeVisible ( list );
	addAndMakeVisible ( line );
	addAndMakeVisible ( view );
}
//-----------------------------------------------------------------------------

void GUI_STILView::setSTIL_blocks ( const juce::String& foldername, const GUI_STIL_blocks& blocks )
{
	//
	// Set list to show tune entries
	//
	list.setBlocks ( blocks );

	//
	// Set text-view for current tune
	//
	view.setViewPosition ( { 0, 0 } );
	view.setBlocks ( blocks );
}
//-----------------------------------------------------------------------------

void GUI_STILView::restorePreferences ()
{
	auto restoreState = [ this ] ( GUI_TagButton& but, const juce::String& setName )
	{
		but.setToggleState ( preferences->get<bool> ( "stil/" + setName ), juce::dontSendNotification );
	};

	restoreState ( toggleTunesOnly, "show-tunes-only" );
	restoreState ( toggleInformation, "show-information" );
	restoreState ( toggleVizAlways, "viz-always" );
}
//-----------------------------------------------------------------------------

void GUI_STILView::setTune ( const juce::String& filename, const int mainTuneNo )
{
	list.setTune ( filename, mainTuneNo );
}
//-----------------------------------------------------------------------------

void GUI_STILView::setTuneLength ( const int tune, const uint32_t lengthMS )
{
	list.setTuneLength ( tune, lengthMS );
}
//-----------------------------------------------------------------------------

void GUI_STILView::setTunePlaying ( const int tune )
{
	list.setTunePlaying ( tune );
	view.setTunePlaying ( tune );
}
//-----------------------------------------------------------------------------

void GUI_STILView::setDefaultTune ( const std::string& title, const int tune )
{
	list.setDefaultTune ( title, tune );
}
//-----------------------------------------------------------------------------

void GUI_STILView::updateFilterStates ()
{
	toggleTunesOnly.setEnabled ( list.hasStingers () && list.hasSongs () );
	toggleInformation.setEnabled ( view.hasInformation () );
}
//-----------------------------------------------------------------------------

void GUI_STILView::timerUpdate ( const float secondsPassed )
{
	list.timerUpdate ( secondsPassed );
}
//-----------------------------------------------------------------------------

juce::Image GUI_STILView::getAuthorImage ( const juce::String& authorPath )
{
	return {};

//	return datasource::loadImage ( "Portraits/Musicians/" + it->second );
}
//-----------------------------------------------------------------------------

juce::Image GUI_STILView::getBugsImage ( const juce::String& author )
{
	return {};

//	return datasource::loadImage ( "Portraits/Bugs/" + it->second );
}
//-----------------------------------------------------------------------------
