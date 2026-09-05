#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Helpers/MipMap.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Database/Database.h"

class Theme;

//-----------------------------------------------------------------------------

class GUI_CoverDisplay final : public juce::Button, public juce::ChangeBroadcaster
{
public:
	GUI_CoverDisplay ();

	// this
	void setImages ( const std::vector<const Database::entry*>& imgs );
	void setImage ( const juce::Image& img );
	[[ nodiscard ]] juce::Colour getAverageColor () const { return averageColor; }

	void setHoverBlend ( float blend );
	[[ nodiscard ]] juce::Image getFinalImage () { return finalMipMap.isValid () ? finalMipMap.getImage () : juce::Image (); }

	std::function<void ()> onPopupMenu;

protected:
	// juce::Component
	void paintButton ( juce::Graphics& g, bool isHover, bool isDown ) override;
	void mouseDown ( const juce::MouseEvent& e ) override;
	void mouseEnter ( const juce::MouseEvent& e ) override;
	void mouseExit ( const juce::MouseEvent& e ) override;

private:
	juce::SharedResourcePointer<Theme>	theme;
	std::vector<const Database::entry*>	images;

	juce::Image		coverImage;

	// Mip-mapped final images, avoids shimmering when drawn smaller
	MipMap			finalMipMap;
	MipMap			finalMipMapHover;
	juce::Colour	averageColor;
	bool			needsUpdate = true;

	// The default-thumbnail version the final image was built from
	int		placeholderVersion = 0;

	float	hoverBlend = 0.0f;

	// Timer-less hover animation, advanced on wall time from within paint
	float	animCur = 0.0f;
	float	animTarget = 0.0f;
	double	lastAnimTime = 0.0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_CoverDisplay )
};
//-----------------------------------------------------------------------------
