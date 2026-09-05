#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_VersionPill.h"

#include "GUI_ultraSID_logo.h"

//-----------------------------------------------------------------------------

class GUI_ultraSID_Badge final : public juce::Component
{
public:
	GUI_ultraSID_Badge ();

	GUI_VersionPill		version;

private:
	GUI_ultraSID_logo	logoUltraSID { "logo", "logos/ultrasid" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ultraSID_Badge )
};
//-----------------------------------------------------------------------------
