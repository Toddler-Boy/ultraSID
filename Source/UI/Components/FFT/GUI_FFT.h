#pragma once

#include <JuceHeader.h>

#include "UI/ui-colors.h"

#include "FFTMeasurement.h"

//-----------------------------------------------------------------------------

// The display half of the FFT: builds and paints the log-frequency curve
// from its app-owned FFTMeasurement source

class GUI_FFT final : public juce::Component
{
public:
	GUI_FFT ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;
	void resized () override				{	rebuildGradient ();	}
	void lookAndFeelChanged () override		{	rebuildGradient ();	}

	// this
	void setSource ( const FFTMeasurement& _source )	{	source = &_source;	}

	void reset ();
	void setBrightness ( const float _brightness );
	void update ();
	void mirror ( const GUI_FFT& source );

	// Theme color ids. A transparent line or fill color skips that part of
	// the drawing
	void setColorIds ( const int _lineColId, const int _fillColId )	{	lineColId = _lineColId;	fillColId = _fillColId;	rebuildGradient ();	}

	void setXResolution ( const int _pixDelta )		{ pixDelta = _pixDelta;	}

	float	brightness = 0.5f;

private:
	int		lineColId = UI::colors::fftLeftLine;
	int		fillColId = UI::colors::fftLeftFill;
	juce::Path				fftPath;
	const juce::Path*		mirroredPath = nullptr;	// borrowed from the mirror() source while the output is mono

	// The themed stroke, resolved per call so hot-reloads land
	[[ nodiscard ]] static juce::PathStrokeType lineStroke ();

	void rebuildGradient ();

	juce::ColourGradient	fillGradient;
	bool					fillVisible = false;

	const FFTMeasurement*	source = nullptr;

	int		pixDelta = 2;

	float	normX[ FFTMeasurement::FFT_SIZE / 2 ] = {};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_FFT )
};
//-----------------------------------------------------------------------------
