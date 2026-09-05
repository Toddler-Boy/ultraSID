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
	// Settings button
	//
	{
		openSettings.margin = 14.0f;
		openSettings.bckAlpha[ 1 ] = 0.1f;
		openSettings.bckMargin = 6.0f;
		openSettings.setSize ( 48, 48 );
		openSettings.setWantsKeyboardFocus ( false );
		openSettings.setMouseClickGrabsKeyboardFocus ( false );

		addAndMakeVisible ( openSettings );
	}

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
