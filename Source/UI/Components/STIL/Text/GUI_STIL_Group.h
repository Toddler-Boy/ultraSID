#pragma once

#include "GUI_STIL_Item.h"

//----------------------------------------------------------------------------------

class GUI_STIL_Group final : public GUI_STIL_Item
{
public:
	GUI_STIL_Group () = default;

	GUI_STIL_Item* addItem ( GUI_STIL_Item* item )
	{
		items.add ( item );
		addAndMakeVisible ( item );

		return item;
	}

	void layout ( int width ) override
	{
		// Layout items
		auto	lastBottom = 0;
		for ( auto y = 0; auto item : items )
		{
			// Layout child
			item->layout ( width );

			if ( ! item->getHeight () )
				continue;

			item->setTopLeftPosition ( 0, y );
			y += item->getHeight () + item->gapBelow ();

			lastBottom = item->getBottom ();
		}

		if ( items.isEmpty () )
			setSize ( width, 0 );
		else
			setSize ( width, lastBottom );
	}

	[[ nodiscard ]] bool isEmpty () const { return items.isEmpty (); }
	[[ nodiscard ]] GUI_STIL_Item* getLastItem () { return items.getLast (); }

private:
	juce::OwnedArray<GUI_STIL_Item> items;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Group )
};
//----------------------------------------------------------------------------------
