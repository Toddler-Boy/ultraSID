#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// The FFT's frequency grid: vertical lines at the reference frequencies
// and/or bottom captions. Split from GUI_FFT so the overlaid stereo curves
// don't each bring their own grid: the sidebar places one lines instance
// BEHIND the curves and one captions instance in front of them.
// The themed colors resolve per paint; a transparent color skips that part
class GUI_FFTGrid final : public juce::Component
{
public:
	GUI_FFTGrid ( const bool _lines, const bool _captions )
		: lines ( _lines )
		, captions ( _captions )
	{
		setBufferedToImage ( true );
		setInterceptsMouseClicks ( false, false );
	}

	// juce::Component
	void paint ( juce::Graphics& g ) override;

private:
	const bool	lines;
	const bool	captions;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_FFTGrid )
};
//-----------------------------------------------------------------------------
