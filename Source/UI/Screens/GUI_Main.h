#pragma once

#include <JuceHeader.h>

#include "UI/Components/Badge/GUI_ultraSID_badge.h"
#include "UI/Components/Footer/GUI_Footer.h"
#include "UI/Components/GUI_AlertBanner.h"
#include "UI/GUI_SidebarLeft.h"
#include "UI/GUI_SidebarRight.h"
#include "UI/Pages/GUI_Pages.h"

//-----------------------------------------------------------------------------

class GUI_Main final : public juce::Component
{
public:
	GUI_Main ( juce::AudioDeviceManager& deviceManager );

	// juce::Component
	void resized () override;

	// The sidebar tiles cache Database::entry pointers too, and live outside GUI_Pages
	void refreshUserTunes ()
	{
		pages.refreshUserTunes ();
		sidebarLeft.refreshPlaylists ();
	}

	// Empty hides it again, so a fixed data file clears the banner by itself
	void showError ( const juce::String& message )
	{
		alertBanner.setMessage ( message );
	}

	gin::LayoutSupport	layout { *this };

	GUI_ultraSID_Badge	badge;
	GUI_Pages			pages;
	GUI_SidebarLeft		sidebarLeft;
	GUI_SidebarRight	sidebarRight;
	GUI_Footer			footer;

private:
	// Declared and added last so it overlays everything else
	GUI_AlertBanner		alertBanner;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Main )
};
//-----------------------------------------------------------------------------
