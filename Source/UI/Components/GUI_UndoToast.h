#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"

//-----------------------------------------------------------------------------

// Message strip for the undo slot, a desktop window above every screen.
// Paint chains repaint for the bar, the timer commits (also while minimized)
class GUI_UndoToast final : public juce::Component
						  , private juce::Timer
{
public:
	GUI_UndoToast ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;
	void mouseEnter ( const juce::MouseEvent& evt ) override;
	void mouseExit ( const juce::MouseEvent& evt ) override;
	void mouseMove ( const juce::MouseEvent& evt ) override;
	void mouseUp ( const juce::MouseEvent& evt ) override;

	// Sizes to content and adds to the desktop, still hidden; the owner
	// positions and shows it
	void setMessage ( const juce::String& msg );

	// Theme reloads don't reach desktop windows, the owner calls this
	void refresh ()		{	setMessage ( message );	}

	// Starts the full undo window over
	void startCountdown ( const int timeoutMS );

	// Undone or committed; hiding alone keeps the op pending
	void stopCountdown ();
	[[ nodiscard ]] bool isPending () const	{	return pending;	}

	std::function<void ()>	onUndo;
	std::function<void ()>	onExpired;

private:
	juce::SharedResourcePointer<Strings>	strings;

	// juce::Timer
	void timerCallback () override;

	juce::String			message;
	juce::Rectangle<int>	undoArea;
	bool					overUndo = false;

	// Countdown; hovering rewinds the bar, leaving restarts in full
	double	startMS = 0.0;
	double	targetMS = 0.0;
	int		durationMS = 0;
	bool	pending = false;
	bool	counting = false;
	float	progress = 0.0f;

	double	rewindStartMS = 0.0;
	float	rewindFrom = 0.0f;
	bool	rewinding = false;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_UndoToast )
};
//-----------------------------------------------------------------------------
