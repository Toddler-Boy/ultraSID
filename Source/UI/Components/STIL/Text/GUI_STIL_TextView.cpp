#include "GUI_STIL_TextView.h"

#include "Resources/STIL_Lookup.h"

#include "GUI_STIL_Bug.h"
#include "GUI_STIL_Comment.h"
#include "GUI_STIL_Group.h"
#include "GUI_STIL_MonoComment.h"
#include "GUI_STIL_Quote.h"
#include "GUI_STIL_Title.h"

//----------------------------------------------------------------------------------

GUI_STIL_TextView::GUI_STIL_TextView ()
{
	setName ( "stilTextView" );

	setScrollBarsShown ( true, false );
	setViewedComponent ( &content, false );
	content.setWantsKeyboardFocus ( false );
	getVerticalScrollBar ().setAutoHide ( false );

	setWantsKeyboardFocus ( true );
	getProperties ().set ( "focusMargin", "2" );
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextView::setBlocks ( const GUI_STIL_blocks& blocks )
{
	tuneGroups.getLock ().enter ();

	content.rootItem = nullptr;
	content.removeAllChildren ();
	tuneGroups.clear ();

	auto	root = new GUI_STIL_Group;
	tuneGroups.add ( root );

	auto	infoGroups = 0;
	auto	infoBlocks = 0;

	auto	group = new GUI_STIL_Group;
	for ( const auto& [ name, value, speaker ] : blocks )
	{
		if ( name == "COMMENT" )
		{
			group->addItem ( new GUI_STIL_Comment ( value ) );
			infoBlocks += infoGroups;
		}
		else if ( name == "QUOTE" )
		{
			group->addItem ( new GUI_STIL_Quote ( value, speaker ) );
			infoBlocks += infoGroups;
		}
		else if ( name == "MONO" )
		{
			group->addItem ( new GUI_STIL_MonoComment ( value ) );
			infoBlocks += infoGroups;
		}
		else if ( name == "TUNE" )
		{
			tuneGroups.add ( group );
			group = new GUI_STIL_Group;
			infoGroups = 1;
		}
		else if ( ( name == "TITLE" || name == "ARTIST" ) && value.isNotEmpty () )
		{
			auto	lastItem = dynamic_cast<GUI_STIL_Title*> ( group->getLastItem () );
			if ( ! lastItem )
			{
				lastItem = static_cast<GUI_STIL_Title*> ( group->addItem ( new GUI_STIL_Title ) );
				infoBlocks += infoGroups;
			}

			lastItem->addEntry ( name, value );
		}
		else if ( name == "BUG" )
		{
			group->addItem ( new GUI_STIL_Bug ( value ) );
			infoBlocks += infoGroups;
		}
	}

	hasInfo = infoBlocks > 0;

	tuneGroups.add ( group );

	content.rootItem = tuneGroups[ 1 ];
	tuneGroups.getLock ().exit ();

	content.addAndMakeVisible ( content.rootItem );

	contentWidth = 0;
	resized ();
}
//-----------------------------------------------------------------------------

void GUI_STIL_TextView::setTunePlaying ( const int tune )
{
	if ( tune < 0 || tune > tuneGroups.size () )
		return;

	content.removeAllChildren ();
	content.rootItem = tuneGroups[ tune + 1 ];
	content.addAndMakeVisible ( content.rootItem );

	contentWidth = 0;
	resized ();
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextView::resized ()
{
	juce::Viewport::resized ();

	// The content only depends on the width; pure height changes (the sidebar
	// balances STIL vs visualizations in two layout passes) need no re-flow
	if ( const auto width = getWidth () - getScrollBarThickness (); width != contentWidth )
	{
		contentWidth = width;
		content.refresh ( width );
	}
}
//----------------------------------------------------------------------------------

void GUI_STIL_TextView::Content::refresh ( int width )
{
	constexpr auto	paddingRight = 0;

	width = std::max ( width, 200 );

	if ( rootItem )
	{
		rootItem->setTopLeftPosition ( 0, 0 );
		rootItem->layout ( width - paddingRight );

		setSize ( width, rootItem->getHeight () );
	}

	repaint ();
}
//----------------------------------------------------------------------------------
