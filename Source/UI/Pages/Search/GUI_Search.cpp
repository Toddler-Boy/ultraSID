#include <JuceHeader.h>

#include "GUI_Search.h"

#include "ultra-shared/Helpers/TextUtils.h"
#include "ultra-shared/Resources/Strings.h"

#include "../GUI_Pages.h"
#include "GUI_Results.h"

//-----------------------------------------------------------------------------

GUI_Search::GUI_Search ( GUI_Pages& pages )
	: results ( pages )
{
	setName ( "search" );

	// Configure search bar
	searchbar.onTextChange = [ this ]
	{
		updateSearch ();
	};

	// Dismissing the search lands the focus on the results
	const auto	focusResults = [ this ]	{	results.grabKeyboardFocus ();	};
	searchbar.getTextEditor ().onReturnPressed = focusResults;
	searchbar.getTextEditor ().onEscapePressed = focusResults;

	info.setName ( "info" );

	addAndMakeVisible ( liked );

	for ( const auto& tag : tags->getTagEntries () )
	{
		auto	tagButton = new GUI_TagButton ( tag.name, tag.colorId );
		tagComps.push_back ( tagButton );
		addAndMakeVisible ( tagButton );

		tagButton->onClick = [ this ]
		{
			updateSearch ();
		};
	}

	// Off shows the outline heart, on the filled one, mirroring how likes look
	liked.stateIcons = { "not-liked", "liked" };

	liked.onClick = [ this ]
	{
		updateSearch ();
	};

	addAndMakeVisible ( info );
	addAndMakeVisible ( results );

	// Add search bar last to be on top
	addAndMakeVisible ( searchbar );
}
//-----------------------------------------------------------------------------

GUI_Search::~GUI_Search ()
{
	for ( auto comp : tagComps )
		delete comp;
}
//-----------------------------------------------------------------------------

void GUI_Search::lookAndFeelChanged ()
{
	auto&	editor = searchbar.getTextEditor ();

	const juce::SharedResourcePointer<Strings>	strings;

	const auto	txtCol = findColour ( UI::colors::text );
	editor.setTextToShowWhenEmpty ( strings->get ( "search/prompt" ), txtCol.withMultipliedAlpha ( 0.25f ) );
	editor.applyColourToAllText ( txtCol );
}
//-----------------------------------------------------------------------------

void GUI_Search::setDatabase ( std::vector<const Database::entry*> db )
{
	results.setDatabase ( std::move ( db ) );
}
//-----------------------------------------------------------------------------

void GUI_Search::setUserDatabase ( std::vector<const Database::entry*> db )
{
	results.setUserDatabase ( std::move ( db ) );
}
//-----------------------------------------------------------------------------

int GUI_Search::search ( const juce::String& str )
{
	searchbar.getTextEditor ().setText ( str, false );
	searchbar.updateClearButton ();

	return updateSearch ();
}
//-----------------------------------------------------------------------------

void GUI_Search::clearFilters ()
{
	liked.setToggleState ( false, juce::dontSendNotification );

	for ( auto comp : tagComps )
		comp->setToggleState ( false, juce::dontSendNotification );
}
//-----------------------------------------------------------------------------

void GUI_Search::showLiked ()
{
	clearFilters ();
	liked.setToggleState ( true, juce::dontSendNotification );

	search ( "" );
}
//-----------------------------------------------------------------------------

int GUI_Search::updateSearch ()
{
	const auto	searchText = searchbar.getTextEditor ().getText ();

	// tagComps follows Tags::getTagEntries (), so it need not stay two entries long
	auto tagState = [ this ] ( const size_t index )
	{
		return index < tagComps.size () && tagComps[ index ]->getToggleState ();
	};

	const auto	numResults = results.search ( searchText, { liked.getToggleState (), tagState ( 0 ), tagState ( 1 ), tagState ( 2 ) } );

	// A filter that's off and matches nothing would only lead to an empty
	// list, so it disables; an active filter always stays usable
	{
		const auto	avail = results.getFilterAvailability ();

		auto enableTag = [ this ] ( const size_t index, const bool available )
		{
			if ( index < tagComps.size () )
				tagComps[ index ]->setEnabled ( tagComps[ index ]->getToggleState () || available );
		};

		liked.setEnabled ( liked.getToggleState () || avail.liked );
		enableTag ( 0, avail.pioneer );
		enableTag ( 1, avail.winner );
		enableTag ( 2, avail.gem );
	}

	const juce::SharedResourcePointer<Strings>	strings;

	if ( numResults )
		info.setText ( strings->get ( numResults == 1 ? "search/result" : "search/results" ).replace ( "{}", textutils::getHumanNumber ( numResults ) ) );
	else
		info.setText ( strings->get ( "search/no_results" ) );

	return numResults;
}
//-----------------------------------------------------------------------------
