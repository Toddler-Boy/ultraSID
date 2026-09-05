#pragma once

#include <JuceHeader.h>

#include "Audio/sid-constants.h"

#include "ChipState/GUI_Filter.h"
#include "ChipState/GUI_Voice.h"
#include "GUI_ChipLabels.h"

//-----------------------------------------------------------------------------

class GUI_ChipState final : public juce::Component
{
public:
	GUI_ChipState ();

	// juce::Component
	void resized () override;
	void paintOverChildren ( juce::Graphics& g ) override;

	// this
	void reset ( const bool ntsc = false, const std::string& model = "6581" );
	void updateState ( uint8_t* regs, const int regIndex );

	[[ nodiscard ]] inline float pitchRegToNote ( const uint16_t _curPitch ) const
	{
		if ( ! _curPitch )
			return -1.0f;

		constexpr auto	constLog2 = 0.693147f;
		return std::log ( _curPitch * clockspeed / 440.0f ) / constLog2 * 12.0f + 69.0f;
	}

	static constexpr auto	minFreq = 30.0f;
	static constexpr auto	maxFreq = 12000.0f;

	[[ nodiscard ]] static inline float freqRegToNormalized ( float freq )
	{
		static const auto start = std::log10 ( minFreq );
		static const auto range = std::log10 ( maxFreq ) - start;

		return ( std::log10 ( freq ) - start ) / range;
	}

	[[ nodiscard ]] std::string getModel () { return model.toStdString (); }

private:
	float			clockspeed;
	juce::String	model;

	GUI_Voice		voices[ SID::numVoices ];
	GUI_Filter		filter;
	GUI_ChipLabels	labels;

	melatonin::InnerShadow	shadow;
	juce::Path				shadowPath;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ChipState )
};
//-----------------------------------------------------------------------------
