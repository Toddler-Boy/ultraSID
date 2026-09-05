#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/Resources/Theme.h"
#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_Line.h"

//-----------------------------------------------------------------------------

class GUI_QualitySelector : public juce::Component
{
public:
	GUI_QualitySelector ();

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;
	bool keyPressed ( const juce::KeyPress& key ) override;

	// this
	void setQuality ( const int quality );

	// Shown as a temporary desktop window that owns the keyboard focus while
	// open: Up/Down move between the qualities, Enter/Space select, Escape closes
	void open ();
	void close ();

	[[ nodiscard ]] bool isOpen () const	{	return isOnDesktop ();	}

	std::function<void ( const int )>	qualityChanged;

	// Keys the selector doesn't use (the global shortcuts keep working while it's open)
	std::function<bool ( const juce::KeyPress& )>	unhandledKey;

private:
	int	quality = 0;

	juce::WeakReference<juce::Component>	previouslyFocused;

	juce::SharedResourcePointer<Theme>	theme;

	juce::Path				shadowPath;
	melatonin::DropShadow	shadow { 12.0 };

	class QualityButton : public juce::ToggleButton
	{
	public:
		QualityButton ( const juce::String& _name, const int colorId );

		// juce::Component
		juce::MouseCursor getMouseCursor () override { return juce::MouseCursor::PointingHandCursor; }

	protected:
		// juce::ToggleButton
		void paintButton ( juce::Graphics& g, bool isHover, bool isDown ) override;

	private:
		int		colorId = 0;
		juce::SharedResourcePointer<Strings>	strings;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( QualityButton )
	};

	GUI_DynamicLabel		qualityLabel { "footer/quality/header", UI::fonts::quality_selector_header };
	QualityButton			qButs[ 5 ] = {
		{ "real", 0 },
		{ "pure", 1 },
		{ "magic", 2 },
		{ "epic", 3 },
		{ "mythic", 4 }
	};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_QualitySelector )
};
//-----------------------------------------------------------------------------
