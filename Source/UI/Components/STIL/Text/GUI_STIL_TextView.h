#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_ViewportSmoothScroll.h"

#include "Resources/STIL_Lookup.h"

#include "GUI_STIL_Group.h"
#include "GUI_STIL_Item.h"

//----------------------------------------------------------------------------------

class GUI_STIL_TextView final : public juce::Viewport
{
public:
	GUI_STIL_TextView ();

	// juce::Viewport
	void resized () override;

	// The items feed themed spacings into their layouts, so a theme switch or
	// hot-reload must re-flow, not just repaint
	void lookAndFeelChanged () override		{	resized ();	}

	// this
	void setBlocks ( const GUI_STIL_blocks& blocks );
	void setTunePlaying ( const int tune );

	[[ nodiscard ]] bool hasInformation () const { return hasInfo; }

	// The laid-out height of the current tune's text blocks; the sidebar uses
	// it to decide whether the visualizations make room
	[[ nodiscard ]] int contentHeight () const { return content.getHeight (); }

private:
	class Content : public juce::Component
	{
	public:
		void refresh ( int width );

		GUI_STIL_Group*	rootItem = nullptr;
	};

	Content		content;
	GUI_ViewportSmoothScroll	smoothScroll { *this };
	juce::OwnedArray<GUI_STIL_Group, juce::CriticalSection>	tuneGroups;
	bool	hasInfo = false;
	int		contentWidth = 0;	// 0 = the next resized () must re-flow

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_TextView )
};
//----------------------------------------------------------------------------------
