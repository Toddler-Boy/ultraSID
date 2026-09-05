#include "GUI_Thumbnail.h"

#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

//-----------------------------------------------------------------------------

GUI_Thumbnail::GUI_Thumbnail ( const juce::String& name )
	: juce::Button ( name )
{
}
//-----------------------------------------------------------------------------

void GUI_Thumbnail::paintButton ( juce::Graphics& g, bool hover, bool /*isButtonDown*/ )
{
	const auto	b = getLocalBounds ().toFloat ();
	const auto	gs = GUI_RoundedClip ( g, b, UI::corner ( UI::corners::footer_thumbnail, b ) );

	const auto	idx = hover ? 1 : 0;

	if ( mipMap[ idx ].isNull () )
		g.fillCheckerBoard ( b, 8.0f, 8.0f, juce::Colours::darkgrey.darker (), juce::Colours::darkgrey );
	else
		mipMap[ idx ].draw ( g, b );
}
//-----------------------------------------------------------------------------

void GUI_Thumbnail::setMipMap ( MipMap& newImage )
{
	mipMap[ 0 ] = newImage;

	auto	hoveredImage = newImage.getImage ().createCopy ();

	gin::applyContrast ( hoveredImage, 20.0f );

	mipMap[ 1 ].setImage ( hoveredImage, newImage.getEnhance () );

	repaint ();
}
//-----------------------------------------------------------------------------
