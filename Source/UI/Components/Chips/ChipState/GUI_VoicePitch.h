#pragma once

#include "GUI_DataHistory.h"

//-----------------------------------------------------------------------------

class GUI_VoicePitch final : public GUI_DataHistory
{
public:
	GUI_VoicePitch ();

	// this
	void addPitch ( const float pitch );

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_VoicePitch )
};
//-----------------------------------------------------------------------------
