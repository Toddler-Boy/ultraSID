#include "GUI_SidebarRight.h"

#include "ultra-shared/Helpers/ComponentUtils.h"
#include "ultra-shared/UI/UI_Helpers.h"


//-----------------------------------------------------------------------------

GUI_SidebarRight::GUI_SidebarRight ()
{
	setName ( "sidebarRight" );

	layout.setConstant ( "singleChip", 1 );
	layout.setConstant ( "twoChips", 0 );
	layout.setConstant ( "chipHalfRows", 0 );

	addAndMakeVisible ( leftBorder );

	addAndMakeVisible ( stil );

	visualizations.setName ( "viz" );

	visualizations.addAndMakeVisible ( memoryOverview );
	visualizations.addAndMakeVisible ( chips );
	visualizations.addAndMakeVisible ( fftGridLines );
	visualizations.addAndMakeVisible ( fftLeft );
	visualizations.addAndMakeVisible ( fftRight );
	visualizations.addAndMakeVisible ( fftGridCaptions );
	visualizations.addAndMakeVisible ( freqLines );

	// Colors resolve from the theme per paint (transparent = part skipped)
	fftGridLines.setName ( "fftGridLines" );
	fftGridCaptions.setName ( "fftGridCaptions" );

	fftLeft.setName ( "fftL" );
	fftLeft.setColorIds ( UI::colors::fftLeftLine, UI::colors::fftLeftFill );

	fftRight.setName ( "fftR" );
	fftRight.setColorIds ( UI::colors::fftRightLine, UI::colors::fftRightFill );

	addAndMakeVisible ( visualizations );
}
//-----------------------------------------------------------------------------

void GUI_SidebarRight::resized ()
{
	auto runLayout = [ this ]
	{
		UI::setLayout ( layout, {	"UI/layouts/constants.json",
								"UI/layouts/sidebar-right.json"
						   } );
	};

	// The first pass with the full stack measures the real text space; the
	// layout places the STIL children directly, so it is always fresh
	layout.setConstant ( "showInfo", stil.showInformation () ? 1 : 0 );
	layout.setConstant ( "stilListMax", stil.listMaxHeight () );
	layout.setConstant ( "keepFull", 1 );
	runLayout ();

	const auto	need = stil.textNeededHeight ();
	const auto	starved = need > 0 && need > stil.textViewHeight ();

	// Chips and memory overview only make room when the text is actually
	// starved and the user hasn't pinned them
	if ( starved && ! stil.vizAlways () )
	{
		layout.setConstant ( "keepFull", 0 );
		runLayout ();
	}

	// The pin only matters while the text is starved
	stil.setVizToggleEnabled ( starved );
}
//-----------------------------------------------------------------------------

void GUI_SidebarRight::setChipsUsed ( const int numChips )
{
	chips.setChipsUsed ( numChips );

	// The chip box is singleChip full rows plus chipHalfRows half-size rows;
	// twoChips only keeps the traditional extra gap above the FFT
	layout.setConstant ( "singleChip", numChips == 1 ? 1 : 0 );
	layout.setConstant ( "twoChips", numChips == 2 ? 1 : 0 );
	layout.setConstant ( "chipHalfRows", numChips == 1 ? 0 : ( numChips + 1 ) / 2 );
	resized ();
}
//-----------------------------------------------------------------------------

void GUI_SidebarRight::likeChanged ()
{
	std::vector<juce::TableListBox*>	tableListBoxes;

	componentutils::getChildrenOfClass<juce::TableListBox> ( this, tableListBoxes );
	for ( auto tlb : tableListBoxes )
		if ( tlb->isVisible () )
			UI::repaintColumn ( tlb, 100 );
}
//-----------------------------------------------------------------------------

void GUI_SidebarRight::showTune ( const SidTuneInfoEZ& info, const int numChips, const bool isNTSC, const bool digiUsed )
{
	for ( auto chipIndex = 0; const auto& model : info.model )
	{
		chips.setModel ( chipIndex, model );
		chips.setProfile ( chipIndex, info.chipProfile, info.chipProfile, info.chipProfileIsApproved );
		chips.getChipState ( chipIndex ).reset ( isNTSC, model );

		++chipIndex;
	}

	setChipsUsed ( numChips );
	chips.setDigiVisible ( digiUsed );

	fftLeft.reset ();
	fftRight.reset ();

	// Set brightness of FFT
	{
		const auto	fftBright = numChips == 1 ? 0.5f : 0.35f;

		fftLeft.setBrightness ( fftBright );
		fftRight.setBrightness ( fftBright );
	}

	freqLines.setChipsUsed ( numChips );
	freqLines.reset ( isNTSC );
}
//-----------------------------------------------------------------------------

void GUI_SidebarRight::timerUpdate ( const float secondsPassed, const float stereoAmount, const bool leftChanged, const bool rightChanged )
{
	stil.timerUpdate ( secondsPassed );

	if ( leftChanged )
		fftLeft.update ();

	// A mono output has nothing to show on the right, so it keeps the left
	// curve mirrored until the two channels actually differ
	if ( rightChanged )
		fftRight.update ();
	else if ( stereoAmount <= 0.0f && leftChanged )
		fftRight.mirror ( fftLeft );
}
//-----------------------------------------------------------------------------

