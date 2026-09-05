#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

//----------------------------------------------------------------------------------

// Author picture of the STIL quote/bug boxes; the owner supplies the image
// via getImage
class GUI_STIL_Portrait final : public juce::Component
{
public:
	GUI_STIL_Portrait ( const bool _iconFallback )
		: iconFallback ( _iconFallback )
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
		else if ( iconFallback )
		{
			const juce::SharedResourcePointer<Icons>	icons;

			// Tinted with the owning box's content color
			const auto	tint = getTint ? getTint () : juce::Colours::white;

			g.fillAll ( tint.withMultipliedAlpha ( 0.33f ) );
			g.setColour ( tint.withMultipliedAlpha ( 0.66f ) );
			g.fillPath ( UI::getScaledPathWithSize ( icons->get ( "portrait-unknown" ), b.translated ( 0.0f, b.getHeight () * 0.1f ), juce::RectanglePlacement::centred ) );
		}
		else
		{
			g.fillAll ( juce::Colours::black );
		}
	}

private:
	bool	iconFallback;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Portrait )
};
//----------------------------------------------------------------------------------
