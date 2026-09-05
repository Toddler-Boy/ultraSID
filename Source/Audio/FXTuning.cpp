#include "FXTuning.h"

//-----------------------------------------------------------------------------

FXTuning& fxTuning ()
{
	static FXTuning	tuning;

	return tuning;
}
//-----------------------------------------------------------------------------

float* FXTuning::slot ( const juce::String& name )
{
	static const std::pair<const char*, float FXTuning::*>	slots[] =
	{
		{ "hum-volume",				&FXTuning::humVolume },
		{ "splitter-freq",			&FXTuning::splitterFreq },
		{ "splitter-low-gain",		&FXTuning::splitterLowGain },
		{ "wide-mono-width",		&FXTuning::wideMonoWidth },
		{ "delay-wet",				&FXTuning::delayWet },
		{ "delay-feedback",			&FXTuning::delayFeedback },
		{ "reverb-wet",				&FXTuning::reverbWet },
		{ "noise-volume",			&FXTuning::noiseVolume },
		{ "noise-color",			&FXTuning::noiseColor },
		{ "epic-wide-mono-width",	&FXTuning::epicWideMonoWidth },
		{ "epic-delay-wet",			&FXTuning::epicDelayWet },
		{ "epic-delay-feedback",	&FXTuning::epicDelayFeedback },
		{ "epic-reverb-wet",		&FXTuning::epicReverbWet },
		{ "mythic-wide-mono-width",	&FXTuning::mythicWideMonoWidth },
		{ "mythic-delay-wet",		&FXTuning::mythicDelayWet },
		{ "mythic-delay-feedback",	&FXTuning::mythicDelayFeedback },
		{ "mythic-reverb-wet",		&FXTuning::mythicReverbWet },
	};

	for ( const auto& [ key, member ] : slots )
		if ( name == key )
			return &( this->*member );

	return nullptr;
}
//-----------------------------------------------------------------------------
