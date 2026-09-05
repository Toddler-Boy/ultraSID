#pragma once

#include <JuceHeader.h>

#include "UI/ui-colors.h"

#include "../chip-constants.h"

//-----------------------------------------------------------------------------

class GUI_DataHistory : public juce::Component
{
public:
	GUI_DataHistory () = default;

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	// this
	void setColorId ( const int colId );
	void reset ();
	void setHistoryLength ( const int length )	{	historyXDelta = 1.0f / float ( length - 1 );	}
	void addDatapoint ( const float data );
	void closePath ();
	[[ nodiscard ]] float getAverage () const					{	return lastResult;	}

private:
	bool	firstPoint = true;
	float	currentX = 1.0f;
	int		colorId = UI::colors::voiceOff;
	float	historyXDelta = 1.0f / float ( UI::chip::numHistory - 1 );
	float	averageValue = 0.0f;
	float	lastValue = 0.0f;
	float	lastResult = 0.0f;
	float	firstValue = 0.0f;

	juce::Path	path;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_DataHistory )
};
//-----------------------------------------------------------------------------
