#pragma once

#include <JuceHeader.h>

#include "GUI_Info.h"
#include "GUI_Transport.h"
#include "GUI_Volume.h"

//-----------------------------------------------------------------------------

// The footer: tune info + thumbnail on the left, transport in the middle,
// volume/quality on the right. The widgets are private, the app talks to
// the footer through the intent-level methods below.

class GUI_Footer : public juce::Component
{
public:
	GUI_Footer ();

	// juce::Component
	void resized () override;
	void paint ( juce::Graphics& g ) override;

	//
	// Transport
	//
	void togglePlay ()						{	transport.play.triggerClick ();	}
	void previousTrack ()					{	transport.previous.triggerClick ();	}
	void nextTrack ()						{	transport.next.triggerClick ();	}
	void toggleShuffle ()					{	transport.shuffle.triggerClick ();	}
	void cycleRepeat ()						{	transport.repeat.triggerClick ();	}
	void seekRelative ( const double seconds )	{	transport.seekRelative ( seconds );	}

	void onSeek ( std::function<void ( int )> handler )		{	transport.seek = std::move ( handler );	}
	void setTransportTime ( const int timeMS, const int renderTimeMS )	{	transport.setTime ( timeMS, renderTimeMS );	}

	[[ nodiscard ]] bool isShuffleOn () const	{	return transport.shuffle.getStage () != 0;	}
	[[ nodiscard ]] int getRepeatStage () const	{	return transport.repeat.getStage ();	}

	void updateTransport ( const bool paused, const bool canPlay, const bool hasPrevious, const bool hasNext, const bool inPlaylist, const int lengthMS );

	//
	// Volume / quality
	//
	void changeVolume ( const double delta )	{	volume.changeVolume ( delta );	}
	void toggleMute ()							{	volume.mute.triggerClick ();	}
	void toggleQualitySelector ()				{	volume.quality.triggerClick ();	}
	void toggleEQPopup ()						{	volume.eq.triggerClick ();	}
	void updatePopupPositions ()				{	volume.updatePopupPositions ();	}
	void restoreVolumePreferences ()	{	volume.restorePreferences ();	}

	void repaintPopups ()
	{
		volume.qualitySelector.repaint ();
		volume.eqPopup.repaint ();
	}

	// Keys an open popup doesn't handle itself
	void onPopupKey ( std::function<bool ( const juce::KeyPress& )> handler )
	{
		volume.qualitySelector.unhandledKey = handler;
		volume.eqPopup.unhandledKey = std::move ( handler );
	}

	[[ nodiscard ]] auto getVolumeState () const	{	return volume.getState ();	}

	// A click outside a transient popup closes it, its own button toggles it
	void dismissPopupsOnOutsideClick ( const juce::Component* clicked, const juce::Point<int> screenPos )
	{
		dismissOnOutsideClick ( volume.qualitySelector, volume.quality, clicked, screenPos );
		dismissOnOutsideClick ( volume.eqPopup, volume.eq, clicked, screenPos );
	}

	// True if one was open
	bool closePopups ()
	{
		const auto	wasOpen = volume.qualitySelector.isOpen () || volume.eqPopup.isOpen ();

		volume.qualitySelector.close ();
		volume.eqPopup.close ();

		return wasOpen;
	}

	// The EQ popup's spectrum, fed only while it's open
	void setFFTSources ( const FFTMeasurement& left, const FFTMeasurement& right )	{	volume.eqPopup.setFFTSources ( left, right );	}
	void spectrumChanged ( const bool stereo )	{	if ( volume.eqPopup.isOpen () ) volume.eqPopup.spectrumChanged ( stereo );	}

	// The peak meters live in the app (they are bound to the effects chain)
	// but render inside the volume area
	void attachMeter ( juce::Component& meter )	{	keepFocusOnClick ( meter );	volume.addChildComponent ( meter );	}

	//
	// Tune info
	//
	void showTuneInfo ( const SidTuneInfoEZ& tuneInfo )	{	info.setStrings ( tuneInfo );	}
	void setThumbnail ( MipMap& mipmap )				{	info.thumbnail.setMipMap ( mipmap );	}

private:
	gin::LayoutSupport	layout { *this };

	GUI_Info		info;
	GUI_Transport	transport;
	GUI_Volume		volume;

	static void keepFocusOnClick ( juce::Component& c );

	static void dismissOnOutsideClick ( GUI_Popup& popup, const juce::Component& button, const juce::Component* clicked, const juce::Point<int> screenPos )
	{
		if ( ! popup.isOpen () || ! popup.closesOnOutsideClick () || clicked == &button || popup.getScreenBounds ().contains ( screenPos ) )
			return;

		popup.close ();
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_Footer )
};
//-----------------------------------------------------------------------------
