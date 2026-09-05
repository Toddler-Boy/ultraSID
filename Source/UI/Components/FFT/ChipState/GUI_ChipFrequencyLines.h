#pragma once

#include <JuceHeader.h>

#include "Audio/sid-constants.h"

#include "../fft-helpers.h"

//-----------------------------------------------------------------------------

class GUI_ChipFrequencyLines final : public juce::Component
{
public:
	GUI_ChipFrequencyLines ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;
	void colourChanged () override;
	void parentHierarchyChanged () override;

	// this
	void reset ( const bool isNTSC );
	void updateState ( uint8_t* regs, const int regIndex );

private:
	[[ nodiscard ]] inline float pitchRegToFreq ( const uint16_t _curPitch ) const
	{
		if ( !_curPitch )
			return -1.0f;

		return _curPitch * clockspeed;
	}

	float	clockspeed;

	struct freqLine
	{
		float	freq = -1.0f;
		float	volume = 0.0f;
		int		colIdx = 0;
	};

	std::array<juce::Colour, 4>		voiceColorTable;

	std::array<std::array<freqLine, SID::numVoices>, UI::fft::numHistory>	lines;

	std::array<juce::Image, 4>		particles;
	int		particleWidth = 0;
	int		particleHeight = 0;

	// The themed width the particles were built with: small width changes can
	// round to the same particleWidth, so the cache tracks it separately
	float	particleLineWidth = 0.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ChipFrequencyLines )
};
//-----------------------------------------------------------------------------
