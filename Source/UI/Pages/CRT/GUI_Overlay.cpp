#include "GUI_Overlay.h"

#include "ultra-shared/Config/DataSource.h"

//-----------------------------------------------------------------------------

GUI_Overlay::GUI_Overlay ()
	: CRTEmulation ( true, 2000,
					 datasource::getCRTRoot (),
					 resolutions {	VIC2_Render::outerUnscaledWidth, VIC2_Render::outerUnscaledHeight,
									VIC2_Render::outerUnscaledWidth * 4, VIC2_Render::outerUnscaledHeight * 4 } )
{
	//enableRenderTimeMeasurement ( true );
	//enableRenderTimeDisplay ( true );

	setName ( "CRT" );

	//
	// Settings and browser buttons
	//
	for ( auto* button : { &openSettings, &openBrowser } )
	{
		button->margin = 14.0f;
		button->bckAlpha[ 1 ] = 0.1f;
		button->bckMargin = 6.0f;
		button->setSize ( 48, 48 );
		button->setWantsKeyboardFocus ( false );
		button->setMouseClickGrabsKeyboardFocus ( false );

		addAndMakeVisible ( *button );
	}

	openBrowser.tooltips = { "crt-browser/browse", "crt-browser/browse" };

	//
	// Page control
	//
	{
		addAndMakeVisible ( pageControl );
	}
}
//-----------------------------------------------------------------------------

void GUI_Overlay::resized ()
{
	CRTEmulation::resized ();

	pageControl.updateLayout ();
}
//-----------------------------------------------------------------------------
