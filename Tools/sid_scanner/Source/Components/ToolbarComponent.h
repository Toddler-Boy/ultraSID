#pragma once

#include <JuceHeader.h>

#include "PatternInputComponent.h"

//-----------------------------------------------------------------------------

// Toolbar strip across the top of the window with its own background colour,
// hosting the pattern input and the database-build controls

class ToolbarComponent final : public juce::Component
{
public:
	ToolbarComponent ();

	PatternInputComponent& getPatternInput ();

	std::function<void ()>		onBuildDatabase;

	// The build-status slot shows either the text or, while a build runs, the
	// progress bar; each call picks its own and hides the other
	void setBuildStatus ( const juce::String& text );
	void setBuildProgress ( float fraction );

	void paint ( juce::Graphics& g ) override;
	void resized () override;

private:
	PatternInputComponent	patternInput;

	juce::Label				buildStatus;
	double					buildProgress = 0.0;
	juce::ProgressBar		progressBar { buildProgress };
	juce::TextButton		buildButton { "Build db" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( ToolbarComponent )
};
//-----------------------------------------------------------------------------
