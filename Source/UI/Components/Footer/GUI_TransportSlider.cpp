#include <JuceHeader.h>

#include "GUI_TransportSlider.h"

#include "std_lime/lime_math.h"

#include "Database/TuneInfo.h"

//-----------------------------------------------------------------------------

GUI_TransportSlider::GUI_TransportSlider ()
	: GUI_Slider ( juce::Slider::LinearHorizontal, juce::Slider::NoTextBox )
{
}
//-----------------------------------------------------------------------------

void GUI_TransportSlider::mouseEnter ( const juce::MouseEvent& e )
{
	GUI_Slider::mouseEnter ( e );
	showPopup ( e );
}
//-----------------------------------------------------------------------------

void GUI_TransportSlider::mouseExit ( const juce::MouseEvent& e )
{
	GUI_Slider::mouseExit ( e );
	hidePopup ();
}
//-----------------------------------------------------------------------------

void GUI_TransportSlider::mouseDrag ( const juce::MouseEvent& e )
{
	GUI_Slider::mouseDrag ( e );
	showPopup ( e );
}
//-----------------------------------------------------------------------------

void GUI_TransportSlider::mouseMove ( const juce::MouseEvent& e )
{
	GUI_Slider::mouseMove ( e );
	showPopup ( e );
}
//-----------------------------------------------------------------------------

void GUI_TransportSlider::showPopup ( const juce::MouseEvent& evt )
{
	// Time is not set
	if ( ! lengthMS || lengthMS == INT32_MAX )
	{
		hidePopup ();
		return;
	}

	const auto	b = getLookAndFeel ().getSliderLayout ( *this ).sliderBounds.toDouble ();
	auto	val = lime::remap ( double ( evt.x ), b.getX (), b.getRight (), 0.0, 1.0 );
	val = std::clamp ( val, 0.0, 1.0 );

	// Update position of PopupDisplay
	{
		const auto	sb = b + getPosition ().toDouble ();

		const auto	halfWidth = popupDisplay.getWidth () / 2;
		const auto	bubbleX = int ( val * sb.getWidth () + sb.getX () ) - halfWidth;

		popupDisplay.setTopLeftPosition ( bubbleX, int ( sb.getY () ) - popupDisplay.getHeight () + 6 );
	}

	// Calculate time based on mouse-position
	{
		const auto	newPosMS = int ( val * lengthMS );
		const auto	str = SID::convertTimeToString ( newPosMS );
		popupDisplay.setText ( str );
	}

	// Fade in PopupDisplay
	if ( ! popupDisplay.isVisible () )
	{
		getParentComponent ()->addChildComponent ( popupDisplay );
		juce::Desktop::getInstance ().getAnimator ().fadeIn ( &popupDisplay, 200 );
	}
}
//-----------------------------------------------------------------------------

void GUI_TransportSlider::hidePopup ()
{
	if ( ! popupDisplay.isVisible () )
		return;

	juce::Desktop::getInstance ().getAnimator ().fadeOut ( &popupDisplay, 200 );

	getParentComponent ()->removeChildComponent ( &popupDisplay );
	popupDisplay.setVisible ( false );
}
//-----------------------------------------------------------------------------
