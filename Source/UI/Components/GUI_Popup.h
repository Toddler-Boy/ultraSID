#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Theme.h"
#include "ultra-shared/UI/Components/GUI_SVG_Button.h"

//-----------------------------------------------------------------------------

// A panel on its own temporary desktop window, owned by the window it opens
// from (always above it, never above other apps). Transient (default): takes
// the keyboard focus, closes on Escape and when the owner reports a click
// outside. staysOpen: never takes the focus (clicks don't activate it),
// closes only via its close button, Escape from the owner, or the owner.
// Keys it doesn't use go to unhandledKey.

class GUI_Popup : public juce::Component
{
public:
	GUI_Popup ( const juce::String& name, const bool staysOpen = false );

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;
	bool keyPressed ( const juce::KeyPress& key ) override;

	// this
	void open ( const juce::Component& owner );
	void close ();

	[[ nodiscard ]] bool isOpen () const	{	return isOnDesktop ();	}
	[[ nodiscard ]] bool closesOnOutsideClick () const	{	return ! staysOpen;	}

	std::function<bool ( const juce::KeyPress& )>	unhandledKey;

	// Window size = panel size + this on every side
	static constexpr int	shadowMargin = 12;

protected:
	[[ nodiscard ]] juce::Rectangle<int> getPanelBounds () const	{	return getLocalBounds ().reduced ( shadowMargin );	}

	// Unmodified keys other than Escape; false hands them to unhandledKey
	virtual bool popupKeyPressed ( const juce::KeyPress& )	{	return false;	}

	virtual void focusOnOpen ()	{	grabKeyboardFocus ();	}

private:
	const bool	staysOpen;

	juce::WeakReference<juce::Component>	previouslyFocused;

	juce::SharedResourcePointer<Theme>	theme;

	juce::Path				shadowPath;
	melatonin::DropShadow	shadow { float ( shadowMargin ) };

	std::unique_ptr<GUI_SVG_Button>	closeButton;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Popup )
};
//-----------------------------------------------------------------------------
