#pragma once

#include <JuceHeader.h>

//----------------------------------------------------------------------------------

// A block in the STIL text view; layout ( width ) sizes the item for the
// given width
class GUI_STIL_Item : public juce::Component
{
public:
	GUI_STIL_Item () = default;

	virtual void layout ( int /*width*/ )	{	setSize ( 0, 0 );	}

	// Vertical gap below this item in the stacked view; the text boxes
	// override this with the themed value
	[[ nodiscard ]] virtual int gapBelow () const	{	return 0;	}

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Item )
};
//----------------------------------------------------------------------------------
