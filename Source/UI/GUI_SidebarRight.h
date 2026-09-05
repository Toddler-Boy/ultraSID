#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/Components/GUI_Line.h"

#include "UI/Components/Chips/GUI_Chips.h"
#include "UI/Components/FFT/GUI_FFT.h"
#include "UI/Components/FFT/GUI_FFTGrid.h"
#include "UI/Components/FFT/GUI_FrequencyLines.h"
#include "UI/Components/MemoryOverview/GUI_MemoryOverview.h"
#include "UI/Components/STIL/GUI_STILView.h"

//-----------------------------------------------------------------------------

// The right sidebar: STIL view on top, tune visualizations (memory map,
// chips, FFTs, frequency lines) below. The widgets are private, the app
// talks to the sidebar through the intent-level methods below.

class GUI_SidebarRight final : public juce::Component
{
public:
	GUI_SidebarRight ();

	// juce::Component
	void resized () override;

	// this
	void setChipsUsed ( const int numChips );
	void likeChanged ();

	// Tune lifecycle
	void showTune ( const SidTuneInfoEZ& info, const int numChips, const bool isNTSC, const bool digiUsed );
	void showMemoryOverview ( const SidTuneInfoEZ& info )	{	memoryOverview.setSidTuneInfo ( info );	}

	// The measurements are owned by the app and shared with the footer spectrum
	void setFFTSources ( const FFTMeasurement& left, const FFTMeasurement& right )
	{
		fftLeft.setSource ( left );
		fftRight.setSource ( right );
	}

	void timerUpdate ( const float secondsPassed, const float stereoAmount, const bool leftChanged, const bool rightChanged );

	void updateChip ( const int chipIndex, uint8_t* regs, const int regIndex )
	{
		chips.getChipState ( chipIndex ).updateState ( regs, regIndex );
		freqLines.updateState ( chipIndex, regs, regIndex );
	}

	void setChipDigiData ( const int chipIndex, int8_t* data, const int lookback )	{	chips.setDigiData ( chipIndex, data, lookback );	}

	// STIL view forwards; whatever changes the text height or the tune list
	// makes the sidebar re-balance STIL vs visualizations right after
	void setTune ( const juce::String& name, const unsigned int mainSong )	{	stil.setTune ( name, mainSong ); resized ();	}
	void setSTIL_blocks ( const juce::String& folder, const auto& blocks )	{	stil.setSTIL_blocks ( folder, blocks ); resized ();	}
	void setTunePlaying ( const int tune )				{	stil.setTunePlaying ( tune ); resized ();	}
	void setTuneLength ( const unsigned int tuneNo, const int lengthMS )	{	stil.setTuneLength ( tuneNo, lengthMS );	}
	void setDefaultTune ( const std::string& title, const unsigned int startSong )	{	stil.setDefaultTune ( title, startSong ); resized ();	}
	void updateFilterStates ()							{	stil.updateFilterStates ();	}
	void restorePreferences ()							{	stil.restorePreferences ();	}

private:
	gin::LayoutSupport	layout { *this };

	GUI_STILView		stil;

	juce::Component	visualizations;
		GUI_MemoryOverview	memoryOverview;
		GUI_Chips			chips;
		GUI_FFTGrid			fftGridLines { true, false };		// behind the curves
		GUI_FFT				fftLeft;
		GUI_FFT				fftRight;
		GUI_FFTGrid			fftGridCaptions { false, true };	// in front of them
		GUI_FrequencyLines	freqLines;

	GUI_Line			leftBorder { "leftBorder" };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_SidebarRight )
};
//-----------------------------------------------------------------------------
