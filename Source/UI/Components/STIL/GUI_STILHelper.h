#pragma once

#include <JuceHeader.h>

//-----------------------------------------------------------------------------

class GUI_STILHelper
{
public:
	[[ nodiscard ]] virtual juce::Image getAuthorImage ( const juce::String& authorPath ) = 0;
	[[ nodiscard ]] virtual juce::Image getBugsImage ( const juce::String& author ) = 0;
};
//-----------------------------------------------------------------------------
