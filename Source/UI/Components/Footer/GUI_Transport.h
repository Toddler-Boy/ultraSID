#pragma once

#include <JuceHeader.h>

#include "Config/Preferences.h"

#include "GUI_TransportButton.h"
#include "GUI_TransportSlider.h"

//-----------------------------------------------------------------------------

class GUI_Transport : public juce::Component
{
public:
	GUI_Transport ();
	~GUI_Transport () override;

	// juce::MouseListener
	void mouseDown ( const juce::MouseEvent& event ) override;
	void mouseDrag ( const juce::MouseEvent& event ) override;

	// this
	void setTime ( int timeMS, int renderTimeMS );
	void setLength ( int timeMS );
	void seekRelative ( double seconds );

	GUI_TransportButton	shuffle { "shuffle", { "footer/player/shuffle", "footer/player/shuffle" } };
	GUI_TransportButton	previous { "prev", { "footer/player/previous" } };
	GUI_TransportButton	play { "play", { "footer/player/pause", "footer/player/play" } };
	GUI_TransportButton	next { "next", { "footer/player/next" } };
	GUI_TransportButton	repeat { "repeat", { "footer/player/repeat", "footer/player/repeat", "footer/player/repeat_1" } };

	juce::Label			time { "time" };
	GUI_TransportSlider	progress;
	juce::Label			length { "length" };

	std::function<void(int)>	seek;

private:
	int		timeMS = INT32_MAX;
	int		renderTimeMS = INT32_MAX;
	int		lengthMS = INT32_MAX;

	juce::SharedResourcePointer<Preferences>	preferences;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Transport )
};
//-----------------------------------------------------------------------------
