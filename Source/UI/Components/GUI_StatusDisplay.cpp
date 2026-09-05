#include "GUI_StatusDisplay.h"

#include "ultra-shared/Resources/Strings.h"

//-----------------------------------------------------------------------------

GUI_StatusDisplay::GUI_StatusDisplay ()
	: juce::Component ( "status" )
{
	label.setName ( "label" );
	progressStr.setName ( "progressStr" );

	progressStr.setJustification ( juce::Justification::centredRight );

	addAndMakeVisible ( label );
	addAndMakeVisible ( progressStr );
	addAndMakeVisible ( progress );
}
//-----------------------------------------------------------------------------

void GUI_StatusDisplay::reset ()
{
	label.setText ( {} );
	progressStr.setText ( {} );
	progress.setProgress ( {} );
}
//-----------------------------------------------------------------------------

void GUI_StatusDisplay::showCancelation ()
{
	const juce::SharedResourcePointer<Strings>	strings;

	label.setText ( strings->get ( "install/canceling" ) );
	progressStr.setText ( {} );
}
//-----------------------------------------------------------------------------

void GUI_StatusDisplay::timerCallback ()
{
	const auto&	state = installState->progress;

	label.setText ( state.getDescription () );
	progressStr.setText ( state.getProgressText () );
	progress.setProgress ( state.getProgress () );
}
//-----------------------------------------------------------------------------
