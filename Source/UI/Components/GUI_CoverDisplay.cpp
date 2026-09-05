#include "GUI_CoverDisplay.h"

#include "ultra-shared/Resources/Theme.h"
#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/ThumbnailCache.h"

//-----------------------------------------------------------------------------

GUI_CoverDisplay::GUI_CoverDisplay ()
	: juce::Button ( "covers" )
{
	setInterceptsMouseClicks ( false, false );

	// Not a tab stop
	setWantsKeyboardFocus ( false );
}
//-----------------------------------------------------------------------------

void GUI_CoverDisplay::paintButton ( juce::Graphics& g, bool /*isHover*/, bool /*isDown*/ )
{
	// Advance hover blend, fading out takes four times as long as fading in
	if ( ! juce::approximatelyEqual ( animCur, animTarget ) )
	{
		const auto	now = juce::Time::getMillisecondCounterHiRes ();
		const auto	step = static_cast<float> ( now - lastAnimTime );
		lastAnimTime = now;

		if ( animTarget > animCur )
			animCur = std::min ( animCur + step / 100.0f, animTarget );
		else
			animCur = std::max ( animCur - step / 400.0f, animTarget );

		hoverBlend = UI::easeInOutQuad ( animCur );
	}

	const juce::SharedResourcePointer<ThumbnailCache>	thmb;

	// The final image may hold copies of the default thumbnail (placeholder
	// or artwork-less tunes), a new default drops them
	if ( placeholderVersion != thmb->getDefaultImageVersion () )
	{
		placeholderVersion = thmb->getDefaultImageVersion ();
		needsUpdate = true;
	}

	// Re-create final image
	{
		const auto	scale = g.getInternalContext ().getPhysicalPixelScaleFactor ();
		const auto	newWidth = static_cast<int> ( 200 * scale );
		const auto	newHeight = static_cast<int> ( 200 * scale );

		if ( newWidth != finalMipMap.getWidth () || newHeight != finalMipMap.getHeight () )
			needsUpdate = true;

		if ( needsUpdate )
		{
			needsUpdate = false;

			// The final image only lives until it is mip-mapped below
			const auto	imgType = coverImage.isValid () ? coverImage.getFormat () : juce::Image::RGB;
			auto	finalImage = juce::Image ( imgType, newWidth, newHeight, imgType == juce::Image::ARGB );

			// Draw into final image
			{
				auto	b = finalImage.getBounds ().toFloat ();
				auto	gi = juce::Graphics ( finalImage );

				if ( coverImage.isValid () )
				{
					gi.setImageResamplingQuality ( juce::Graphics::highResamplingQuality );
					gi.drawImage ( coverImage, b, juce::RectanglePlacement::fillDestination );
				}
				else
				{
					if ( images.empty () )
					{
						constexpr auto	w = 88;
						constexpr auto	h = static_cast<int> ( w * VIC2::truePalX );

						constexpr auto	borderPadding = 10;
						const auto	placeholder = thmb->getDefaultScreen ().getClippedImage ( { VIC2_Render::unscaledBorderSizeX - borderPadding, VIC2_Render::unscaledBorderSizeY - borderPadding, w, h } );

						gi.setImageResamplingQuality ( juce::Graphics::lowResamplingQuality );
						gi.drawImage ( placeholder, b );
					}
					else
					{
						constexpr auto	numCovers = 4;
						constexpr auto	numPerRow = 2;

						const auto		thmbH = b.getHeight () / numPerRow;

						auto	thmbRow = b.removeFromTop ( thmbH );

						// Always draw four covers
						for ( auto i = 0; i < numCovers; ++i )
						{
							const auto	idx = i % images.size ();
							const auto	thmbR = thmbRow.removeFromLeft ( thmbH );

							// Draw thumbnail
							{
								auto	ss = GUI_RoundedClip ( gi, thmbR, 0.0f );

								auto&	ent = images[ idx ];
								auto&	mipMap = thmb->getThumbnail ( ent->file, ent->isNTSC (), [ safe = juce::Component::SafePointer<GUI_CoverDisplay> ( this ) ] {
									// The component may be gone by the time the render job finishes
									if ( safe == nullptr )
										return;

									safe->needsUpdate = true;
									safe->repaint ();
								} );

								mipMap.draw ( gi, thmbR, juce::RectanglePlacement::fillDestination );
							}

							// Next row
							if ( thmbRow.getWidth () < 1.0f )
								thmbRow = b.removeFromTop ( thmbH );
						}
					}
				}
			}

			// Create hover-image
			auto	finalImageHover = finalImage.createCopy ();
			gin::applyContrast ( finalImageHover, 20.0f );

			// Re-create mip-maps; only the tiles composed from VIC2 renders get the enhancement
			const auto	enhance = coverImage.isValid () ? MipMap::Enhance {} : ThumbnailCache::vic2Enhance;

			finalMipMap.setImage ( finalImage, enhance );
			finalMipMapHover.setImage ( finalImageHover, enhance );

			// Get average color
			averageColor = UI::getAverageColor ( finalImage, -1.0f, 1.5f, 1.0f );

			sendChangeMessage ();
		}
	}

	// Draw final image
	{
		const auto	b = getLocalBounds ().toFloat ();
		const auto	gs = GUI_RoundedClip ( g, b, UI::corner ( UI::corners::playlist_cover, b ) );

		if ( juce::approximatelyEqual ( hoverBlend, 1.0f ) )
		{
			finalMipMapHover.draw ( g, b );
		}
		else
		{
			finalMipMap.draw ( g, b );
			if ( hoverBlend > 0.0f )
			{
				g.setOpacity ( hoverBlend );
				finalMipMapHover.draw ( g, b );
			}
		}
	}

	// Still blending, paint again on the next v-blank
	if ( ! juce::approximatelyEqual ( animCur, animTarget ) )
		repaint ();
}
//-----------------------------------------------------------------------------

void GUI_CoverDisplay::mouseDown ( const juce::MouseEvent& e )
{
	if ( ! e.mods.isPopupMenu () )
	{
		juce::Button::mouseDown ( e );
		return;
	}

	if ( onPopupMenu )
		onPopupMenu ();
}
//-----------------------------------------------------------------------------

void GUI_CoverDisplay::mouseEnter ( const juce::MouseEvent& e )
{
	animTarget = 1.0f;
	lastAnimTime = juce::Time::getMillisecondCounterHiRes ();
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_CoverDisplay::mouseExit ( const juce::MouseEvent& e )
{
	animTarget = 0.0f;
	lastAnimTime = juce::Time::getMillisecondCounterHiRes ();
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_CoverDisplay::setImages ( const std::vector<const Database::entry*>& imgs )
{
	images = imgs;
	coverImage = {};
	needsUpdate = true;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_CoverDisplay::setImage ( const juce::Image& img )
{
	images = {};
	coverImage = img;
	needsUpdate = true;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_CoverDisplay::setHoverBlend ( float blend )
{
	hoverBlend = blend;
	repaint ();
}
//-----------------------------------------------------------------------------
