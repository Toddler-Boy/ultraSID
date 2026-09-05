#include <JuceHeader.h>

#include <fmt/format.h>

#include "GUI_MemoryOverview.h"

#include "std_lime/lime_math.h"

#include "ultra-shared/Helpers/TextUtils.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

//-----------------------------------------------------------------------------

GUI_MemoryOverview::GUI_MemoryOverview ()
{
	setName ( "memoryOverview" );

	setBufferedToImage ( true );
}
//-----------------------------------------------------------------------------

void GUI_MemoryOverview::paint ( juce::Graphics& g )
{
	const auto	b = getLocalBounds ().toFloat ();
	const auto	sg = GUI_RoundedClip ( g, b, UI::corner ( UI::corners::memory_overview, b ) );

	// Dark blue
	g.fillAll ( juce::Colour ( 0xff2e2c9b ) );

	auto showRegion = [ &g, this ] ( const uint16_t address, const uint32_t length, const bool top, const juce::Colour col )
	{
		if ( ! length )
			return;

		const auto	w = float ( getWidth () );
		const auto	h = float ( getHeight () );

		auto	x1 = lime::remap ( float ( address ), 0.0f, 65536.0f, 0.0f, w );
		auto	x2 = lime::remap ( float ( address + length - 1 ), 0.0f, 65536.0f, 0.0f, w );

		juce::Rectangle<float>	region = { x1, 0.0f, x2 - x1, h };

		if ( top )
			region = region.withTrimmedBottom ( h * 0.5f );

		g.setColour ( col );
		g.fillRect ( region );
	};

	// Tune itself
	showRegion ( info.c64LoadAddress, info.c64DataLength, false, juce::Colour ( 0xff706deb ) );	// Light blue

	// Basic
	showRegion ( 0xA000, 0x2000, true, juce::Colours::yellow );

	// Chargen
	showRegion ( 0xD000, 0x1000, true, juce::Colours::orange );

	// Kernal
	showRegion ( 0xE000, 0x2000, true, juce::Colours::coral );

	// Zero page
	showRegion ( 0x0000, 0x0100, true, juce::Colours::red );

	// Enhanced zero page and stack
	showRegion ( 0x0100, 0x0100, true, juce::Colours::deeppink );
}
//-----------------------------------------------------------------------------

juce::String GUI_MemoryOverview::getTooltip ()
{
	if ( ! info.c64DataLength )
		return strings->get ( "memory/nothing_loaded_tip" );

	// The themed strings are fmt patterns, so a translation can reorder the
	// values ("{0:04X}" style)
	auto format = [ this ] ( const char* key, auto&&... args )
	{
		const auto	pattern = strings->get ( key ).toStdString ();
		return fmt::format ( fmt::runtime ( pattern ), std::forward<decltype ( args )> ( args )... );
	};

	auto	tip = format ( "memory/load_address", info.c64LoadAddress, info.c64LoadAddress + info.c64DataLength - 1,
						   textutils::getHumanNumber ( info.c64DataLength ).toStdString () ) + "\n";

	if ( info.c64InitAddress )
		tip += format ( "memory/init_address", info.c64InitAddress );
	else
		tip += strings->get ( "memory/basic_program" ).toStdString ();

	if ( info.c64PlayAddress )
		tip += "\n" + format ( "memory/play_address", info.c64PlayAddress );

	if ( ! info.playroutineID.empty () )
		for ( const auto& pr : info.playroutineID )
			tip += "\n" + format ( "memory/playroutine", pr );

	tip += "\n" + info.speed;

	return tip;
}
//-----------------------------------------------------------------------------

void GUI_MemoryOverview::setSidTuneInfo ( const SidTuneInfoEZ& _info )
{
	info = _info;
	repaint ();
}
//-----------------------------------------------------------------------------
