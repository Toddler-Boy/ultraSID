#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// The audio-enhancement tuning in slider units (0..100): one process-wide set,
// never stored. User builds run these numbers, developer builds expose them as
// sliders on the settings page

struct FXTuning
{
	float	humVolume = 7.0f;

	float	splitterFreq = 20.0f;
	float	splitterLowGain = 15.0f;

	float	wideMonoWidth = 50.0f;
	float	delayWet = 17.0f;
	float	delayFeedback = 42.0f;
	float	reverbWet = 32.0f;

	float	noiseVolume = 5.0f;
	float	noiseColor = 50.0f;

	float	epicWideMonoWidth = 80.0f;
	float	epicDelayWet = 31.0f;
	float	epicDelayFeedback = 55.0f;
	float	epicReverbWet = 45.0f;

	float	mythicWideMonoWidth = 100.0f;
	float	mythicDelayWet = 46.0f;
	float	mythicDelayFeedback = 71.0f;
	float	mythicReverbWet = 60.0f;

	// The value behind a settings-page name ("delay-wet"), nullptr for an unknown one
	[[ nodiscard ]] float* slot ( const juce::String& name );
};
//-----------------------------------------------------------------------------

[[ nodiscard ]] FXTuning& fxTuning ();
//-----------------------------------------------------------------------------
