#pragma once

#include <JuceHeader.h>

class GUI_VoiceWaveforms final : public juce::Component
{
public:
	GUI_VoiceWaveforms ( const int shape );

	// JUCE
	void paint ( juce::Graphics& g ) override;

	// this
	void setState ( const bool enabled, const uint16_t pitch, const float note, const uint16_t pw, const int index );

private:
	juce::Path		path;
	bool			on = false;
	int				shape;
	uint16_t		curPitch = 0;
	float			curNote = 0.0f;
	uint16_t		curPW = 0;
	int				curIndex = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_VoiceWaveforms )
};
//-----------------------------------------------------------------------------
