#include <JuceHeader.h>

#include "GUI_Footer.h"

#include "ultra-shared/UI/UI_Helpers.h"

//-----------------------------------------------------------------------------

GUI_Footer::GUI_Footer ()
{
	setOpaque ( true );
	setName ( "footer" );

	addAndMakeVisible ( info );
	addAndMakeVisible ( transport );
	addAndMakeVisible ( volume );

	// Clicking a footer control leaves the keyboard focus where it is (e.g. on
	// the list box); keyboard navigation into the footer stays possible. The
	// quality selector is exempt, it owns the focus while open
	keepFocusOnClick ( *this );
}
//-----------------------------------------------------------------------------

void GUI_Footer::keepFocusOnClick ( juce::Component& c )
{
	c.setMouseClickGrabsKeyboardFocus ( false );

	for ( auto* child : c.getChildren () )
		keepFocusOnClick ( *child );
}
//-----------------------------------------------------------------------------

void GUI_Footer::resized ()
{
	UI::setLayout ( layout, {	"UI/layouts/constants.json",
								"UI/layouts/footer.json",
							} );
}
//-----------------------------------------------------------------------------

void GUI_Footer::paint ( juce::Graphics& g )
{
	g.fillAll ( UI::getShade ( 0.1f ) );
}
//-----------------------------------------------------------------------------

void GUI_Footer::updateTransport ( const bool paused, const bool canPlay, const bool hasPrevious, const bool hasNext, const bool inPlaylist, const int lengthMS )
{
	transport.play.setStage ( paused ? 1 : 0 );
	transport.play.setEnabled ( canPlay );

	transport.previous.setEnabled ( hasPrevious );
	transport.next.setEnabled ( hasNext );

	transport.shuffle.setEnabled ( inPlaylist );

	transport.setLength ( lengthMS );
}
//-----------------------------------------------------------------------------
