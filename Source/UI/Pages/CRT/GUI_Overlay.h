#pragma once

#include <JuceHeader.h>

#include <chrono>

#include "ultra-shared/UI/Components/GUI_SVG_Button.h"
#include "ultra-shared/Video/colodore.h"
#include "ultra-shared/Video/VIC2_Render.h"

#include "Config/Preferences.h"
#include "UI/Components/GUI_PaginationDots.h"

//#include "C64u_UDP_Receiver.h"

//-----------------------------------------------------------------------------

class GUI_Overlay final : public lime::CRTEmulation
{
public:
	GUI_Overlay ();

	// juce::Component
	void resized () override;

	// this
	void setNumCRTpages ( const int numPages )	{	pageControl.setNumberOfPages ( numPages );	}
	void setCRTPage ( const int page )			{	pageControl.setCurrentPage ( page, true );	}
	[[ nodiscard ]] int getCRTPage () const						{	return pageControl.getCurrentPage ();		}

	GUI_SVG_Button	openSettings { "open", { "crt/settings_close", "crt/settings_open" } };

private:
	//
	// Show number of images
	//
	GUI_PaginationDots		pageControl;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Overlay )
};
//-----------------------------------------------------------------------------
