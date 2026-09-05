#include "GUI_ultraSID_badge.h"

#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

GUI_ultraSID_Badge::GUI_ultraSID_Badge ()
	: juce::Component ( "badge" )
{
	setInterceptsMouseClicks ( false, true );

	addAndMakeVisible ( logoUltraSID );
	addAndMakeVisible ( version );

	logoUltraSID.onClick = []	{	msg::ShowAbout {}.send ();	};
}
//-----------------------------------------------------------------------------
