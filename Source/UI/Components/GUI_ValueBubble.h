#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

// A small floating readout above a dragged or hovered control (transport
// position, EQ handle dB); the owner positions it and feeds the text

class GUI_ValueBubble : public juce::Component
{
public:
	GUI_ValueBubble ();

	void paint ( juce::Graphics& g ) override;
	void setFont ( const juce::Font& _font ) { font = _font; }
	void setText ( const juce::String& _text ) { text = _text; repaint (); }

private:
	juce::Font		font { juce::FontOptions {} };
	juce::String	text;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ValueBubble )
};
//-----------------------------------------------------------------------------
