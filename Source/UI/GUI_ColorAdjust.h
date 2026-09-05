#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Label.h"
#include "ultra-shared/UI/Components/GUI_Slider.h"

#include "Config/Preferences.h"
#include "UI/Components/GUI_TextButton.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

// The hidden color-adjustment window (Ctrl+Shift+F10): grades every themed
// color live (gamma, brightness, contrast and saturation) and persists the
// values as preferences

class GUI_ColorAdjust final : public juce::DocumentWindow
{
public:
	GUI_ColorAdjust ( std::function<void ()> _onChanged, std::function<void ()> _onClosed )
		: juce::DocumentWindow ( "Color adjustments", juce::Colours::black, juce::DocumentWindow::closeButton )
		, onChanged ( std::move ( _onChanged ) )
		, onClosed ( std::move ( _onClosed ) )
	{
		setUsingNativeTitleBar ( true );
		setBackgroundColour ( juce::LookAndFeel::getDefaultLookAndFeel ().findColour ( UI::colors::window ) );

		panel.setSize ( 380, 4 * ( Panel::rowH + Panel::gap ) + Panel::rowH + 2 * Panel::margin );
		setContentNonOwned ( &panel, true );

		setResizable ( false, false );
		setAlwaysOnTop ( true );
		centreWithSize ( getWidth (), getHeight () );
		setVisible ( true );
	}

	// juce::DocumentWindow
	void closeButtonPressed () override	{	if ( onClosed ) onClosed ();	}

private:
	// A label-plus-slider line bound to its "ui/" preference
	struct SliderRow final : juce::Component
	{
		SliderRow ( const juce::String& stringKey, const juce::String& _prefKey, const double min, const double max )
			: prefKey ( _prefKey )
			, label ( stringKey, 13.0f, 500 )
		{
			slider.setRange ( min, max, 0.0 );
			slider.setNumDecimalPlacesToDisplay ( 2 );
			slider.setDoubleClickReturnValue ( true, 1.0 );
			slider.setTextBoxStyle ( juce::Slider::TextBoxRight, false, 48, 20 );

			const juce::SharedResourcePointer<Preferences>	preferences;
			slider.setValue ( preferences->get<double> ( prefKey ), juce::dontSendNotification );

			addAndMakeVisible ( label );
			addAndMakeVisible ( slider );
		}

		// juce::Component
		void resized () override
		{
			auto	b = getLocalBounds ();

			label.setBounds ( b.removeFromLeft ( labelWidth ) );
			slider.setBounds ( b );
		}

		static constexpr auto	labelWidth = 100;

		juce::String		prefKey;
		GUI_DynamicLabel	label;
		GUI_Slider			slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
	};

	std::function<void ()>	onChanged;
	std::function<void ()>	onClosed;

	//
	// Content
	//
	class Panel final : public juce::Component
	{
	public:
		explicit Panel ( GUI_ColorAdjust& _owner )
			: owner ( _owner )
		{
			const juce::SharedResourcePointer<Preferences>	preferences;

			for ( auto* row : { &gamma, &brightness, &contrast, &saturation } )
			{
				row->slider.onValueChange = [ this, row, preferences ]
				{
					preferences->set ( row->prefKey, row->slider.getValue () );

					if ( owner.onChanged )
						owner.onChanged ();
				};

				addAndMakeVisible ( *row );
			}

			reset.onClick = [ this ]
			{
				// Each slider pushes its preference and re-applies
				for ( auto* row : { &gamma, &brightness, &contrast, &saturation } )
					row->slider.setValue ( 1.0, juce::sendNotification );
			};

			addAndMakeVisible ( reset );
		}

		// juce::Component
		void resized () override
		{
			auto	b = getLocalBounds ().reduced ( margin );

			for ( auto* row : { &gamma, &brightness, &contrast, &saturation } )
			{
				row->setBounds ( b.removeFromTop ( rowH ) );
				b.removeFromTop ( gap );
			}

			reset.setBounds ( b.removeFromTop ( rowH ).removeFromRight ( 90 ) );
		}

		static constexpr auto	rowH = 28;
		static constexpr auto	gap = 4;
		static constexpr auto	margin = 12;

		GUI_ColorAdjust&	owner;

		SliderRow	gamma		{ "color-adjust/gamma",			"ui/gamma",			0.5, 2.0 };
		SliderRow	brightness	{ "color-adjust/brightness",	"ui/brightness",	0.5, 1.5 };
		SliderRow	contrast	{ "color-adjust/contrast",		"ui/contrast",		0.5, 1.5 };
		SliderRow	saturation	{ "color-adjust/saturation",	"ui/saturation",	0.0, 2.0 };

		GUI_TextButton	reset { "reset", "color-adjust/reset" };
	};

	Panel	panel { *this };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_ColorAdjust )
};
//-----------------------------------------------------------------------------
