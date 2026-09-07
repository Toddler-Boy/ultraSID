#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/Components/GUI_PortraitPlaceholder.h"

//----------------------------------------------------------------------------------

// Author picture of the STIL quote/bug boxes; the owner supplies the image
// via getImage
class GUI_STIL_Portrait final : public juce::Component
{
public:
	GUI_STIL_Portrait ()
	{
		setName ( "portrait" );
		setInterceptsMouseClicks ( false, false );
	}

	std::function<juce::Image ()>	getImage;

	// The owning box's content color, the same funnel its text draws with
	std::function<juce::Colour ()>	getTint;

	void paint ( juce::Graphics& g ) override
	{
		const auto	b = getLocalBounds ().toFloat ();
		const auto	gs = GUI_RoundedClip ( g, b, UI::corner ( UI::corners::stil_portrait, b ) );

		if ( const auto img = getImage ? getImage () : juce::Image (); img.isValid () )
		{
			g.setImageResamplingQuality ( juce::Graphics::highResamplingQuality );
			g.drawImage ( img, b, juce::RectanglePlacement::fillDestination );
		}
		else
		{
			placeholder.draw ( g, b, getTint ? getTint () : juce::Colours::white );
		}
	}

private:
	GUI_PortraitPlaceholder	placeholder;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Portrait )
};
//----------------------------------------------------------------------------------
