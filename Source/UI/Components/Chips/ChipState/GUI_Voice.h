#pragma once

#include <JuceHeader.h>

#include "GUI_VoiceControl.h"
#include "GUI_VoicePitch.h"
#include "GUI_VoiceWaveforms.h"

//-----------------------------------------------------------------------------

//class GUI_ultraSID;

class GUI_Voice final : public juce::Component
{
public:
	GUI_Voice ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;
	void lookAndFeelChanged () override;

	// this
	void setState ( const int index, const uint8_t shapes, const uint16_t pitch, const float note, const uint16_t pw, const bool filtered, const bool _muted );
	void setPitch ( const int index, const float note );
	void setEnvelope ( const int index, const uint8_t value );

	// The stacked voice rows form ONE visual box: only the outer rows round
	// their outer corners, the middle row stays square
	void setRoundedEnds ( const bool _top, const bool _bottom )		{	roundTop = _top;	roundBottom = _bottom;	}

private:
	void setVoiceColors ();

	bool	roundTop = true;
	bool	roundBottom = true;

	bool	filtered = false;
	bool	muted = false;
	float	envLevel = 0.0f;

	GUI_VoicePitch			pitchDisplay;
	GUI_VoiceControl		control;
	GUI_DataHistory			envelope;
	GUI_VoiceWaveforms		oscTri { 0 };
	GUI_VoiceWaveforms		oscSaw { 1 };
	GUI_VoiceWaveforms		oscPls { 2 };
	GUI_VoiceWaveforms		oscNse { 3 };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Voice )
};
//-----------------------------------------------------------------------------
