#pragma once

#include <JuceHeader.h>

#include "ultra-shared/Resources/Strings.h"

#include "Config/Preferences.h"
#include "UI/Components/GUI_ValueBubble.h"

class FFTMeasurement;

//-----------------------------------------------------------------------------

// The user's global tone preference on the settings page: one 3-band
// offset curve (-1..+1 per band, SIDEffects maps it to dB) applied on top of
// every mode's preset EQ, REAL included, their adaptation to the listening
// environment. Drawn as a line through three equal-width low/mid/high bands
// with one vertically draggable handle centered in each; deliberately not a
// frequency axis.

class GUI_EQCurve final : public juce::Component
{
public:
	GUI_EQCurve ();

	// juce::Component
	void resized () override;

	// this
	void restorePreferences ()	{	curve.restorePreferences ();	}

	// The measurements are owned by the app, shared with the sidebar FFTs
	void setFFTSources ( const FFTMeasurement& left, const FFTMeasurement& right )	{	spectrum.setSources ( left, right );	}
	void spectrumChanged ( const bool stereo )	{	spectrum.dataChanged ( stereo );	}

private:
	// The static scenery: a dashed center line per band with the band names
	// underneath. Buffered to an image, it only repaints on resize
	class Backdrop final : public juce::Component
	{
	public:
		Backdrop ();

		void paint ( juce::Graphics& g ) override;

	private:
		juce::SharedResourcePointer<Strings>	strings;
	};

	// The low-resolution spectrum behind the curve: one pill per themed band,
	// left channel up from the center line, right channel down, mono mirrored
	class Spectrum final : public juce::Component
	{
	public:
		Spectrum ();

		void paint ( juce::Graphics& g ) override;
		void resized () override				{	rebuildGradient ();	}
		void lookAndFeelChanged () override		{	rebuildGradient ();	}

		void setSources ( const FFTMeasurement& _left, const FFTMeasurement& _right )	{	left = &_left;	right = &_right;	}
		void dataChanged ( const bool _stereo )	{	stereo = _stereo;	repaint ();	}

	private:
		void rebuildGradient ();

		juce::ColourGradient	gradient;

		const FFTMeasurement*	left = nullptr;
		const FFTMeasurement*	right = nullptr;

		bool	stereo = false;
	};

	// The interactive part: the offset line with one draggable dot per band,
	// vertically centered inside the backdrop's scenery
	class Curve final : public juce::Component
	{
	public:
		Curve ();

		// juce::Component
		void paint ( juce::Graphics& g ) override;
		void resized () override;
		void lookAndFeelChanged () override;

		// this
		void restorePreferences ();

	private:
		class Handle final : public juce::Component
		{
		public:
			Handle ( Curve& owner, const int bandIndex );

			// The dots are painted by Curve itself, on top of the line
			void mouseDown ( const juce::MouseEvent& e ) override;
			void mouseDrag ( const juce::MouseEvent& e ) override;
			void mouseEnter ( const juce::MouseEvent& e ) override;
			void mouseExit ( const juce::MouseEvent& e ) override;

		private:
			Curve&		owner;
			const int	band;
		};

		void handleDragged ( const int band, const juce::Point<float> pos );
		void resetOffset ( const int band );
		void publishBand ( const int band );
		void layoutHandles ();
		void rebuildCurve ();

		// The dB bubble above the hovered or dragged handle, hosted outside
		// the curve so it survives the top-of-travel clip
		void showBubble ( const int band );
		void hideBubble ();

		[[ nodiscard ]] juce::String offsetText ( const int band ) const;

		[[ nodiscard ]] static juce::String offsetKey ( const int band );

		// The hot/neutral/cold colors over the vertical travel
		[[ nodiscard ]] juce::ColourGradient temperatureGradient ( const float alpha ) const;

		juce::ColourGradient	gradient;

		// Pixels from the centre line to full deflection
		[[ nodiscard ]] float halfTravel () const;

		[[ nodiscard ]] float offsetToY ( const float offset ) const;
		[[ nodiscard ]] float yToOffset ( const float y ) const;

		juce::SharedResourcePointer<Preferences>	preferences;

		float	bandOffset[ 3 ] = { 0.0f, 0.0f, 0.0f };

		juce::Path	intentCurve;	// illustrative line through the handles, not the true response

		GUI_ValueBubble	bubble;

		std::unique_ptr<Handle>	handles[ 3 ];

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( Curve )
	};

	Backdrop	backdrop;
	Spectrum	spectrum;
	Curve		curve;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_EQCurve )
};
//-----------------------------------------------------------------------------
