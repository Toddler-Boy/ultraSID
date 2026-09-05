#pragma once

#include <JuceHeader.h>

#include <functional>
#include <optional>

//-----------------------------------------------------------------------------

// Single-slot undo: the action applies immediately, the destructive step
// waits in commit. The UI owns the countdown and flushes on timeout
class UndoManager final
{
public:
	struct Op
	{
		juce::String			text;	// toast message
		std::function<void ()>	commit;	// deferred destructive step, may be empty
		std::function<void ()>	undo;	// puts data and views back
	};

	UndoManager () = default;
	~UndoManager ();

	// Commits any armed op first
	void arm ( Op op );

	void undo ();
	void flush ();

	// Wired once by the main UI, both optional
	std::function<void ( const juce::String& )>	onShow;
	std::function<void ()>						onHide;

	static constexpr int	timeoutMS = 5000;

private:
	std::optional<Op>	current;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( UndoManager )
};
//-----------------------------------------------------------------------------
