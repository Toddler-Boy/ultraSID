#pragma once

#include <JuceHeader.h>

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

class GUI_DigiDisplay : public juce::Component
{
public:
	GUI_DigiDisplay ();

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;

	// this: data arrives as display-ready signed samples, the engine's digi
	// capture does all technique-specific conversion. data points into the
	// rolling waveform buffer, lookback = valid samples available before it
	void setColorId ( const int colId );
	void reset ();
	void setData ( const int8_t* data, const int lookback );

	static constexpr int	blockLength = 44100 / 60;

private:
	melatonin::InnerShadow	shadow;
	juce::Path				shadowPath;

	int		colorId = UI::colors::digi;

	void drawWindow ( const int8_t* window );

	// Waveform lock: the drawn window slides backwards through the waveform
	// history to the best match with the previously drawn one, so periodic
	// content stands still. Everything reads the caller's buffer inside
	// setData only; paint just draws the finished path
	std::array<int8_t, blockLength>	lockRef {};
	bool	hasLockRef = false;

	// Scope trigger for sparse content: a quiet stretch arms it, the next
	// sample leaving the quiet level fires and anchors the window so the
	// edge sits a few pixels in from the left instead of a random spot
	bool	armed = false;
	int8_t	quietLevel = 0;
	int		pendingAnchor = -1;
	int		quietFrames = 0;

	// No processing without a play-position change (pause, stopped)
	int		lastOffset = -1;

	juce::Path	path;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_DigiDisplay )
};
//-----------------------------------------------------------------------------
