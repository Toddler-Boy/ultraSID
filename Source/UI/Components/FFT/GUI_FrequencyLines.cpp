#include "GUI_FrequencyLines.h"

#include "fft-helpers.h"


//-----------------------------------------------------------------------------

GUI_FrequencyLines::GUI_FrequencyLines ()
{
	setName ( "frequencyLines" );
	setInterceptsMouseClicks ( false, false );
}
//-----------------------------------------------------------------------------

void GUI_FrequencyLines::ensureChips ( size_t count )
{
	while ( chips.size () < count )
		addChildComponent ( *chips.emplace_back ( std::make_unique<GUI_ChipFrequencyLines> () ) );
}
//-----------------------------------------------------------------------------

GUI_ChipFrequencyLines& GUI_FrequencyLines::chipAt ( size_t chipNo )
{
	ensureChips ( chipNo + 1 );

	return *chips[ chipNo ];
}
//-----------------------------------------------------------------------------

void GUI_FrequencyLines::resized ()
{
	if ( chipsUsed <= 0 )
		return;

	auto	b = getLocalBounds ().reduced ( 0, int ( UI::fft::glow ) );

	const auto	ch = b.getHeight () / chipsUsed;

	for ( auto& c : chips )
		c->setBounds ( b.removeFromTop ( ch ).expanded ( 0, int ( UI::fft::glow ) ) );
}
//-----------------------------------------------------------------------------

void GUI_FrequencyLines::setChipsUsed ( int count )
{
	chipsUsed = count;

	if ( count > 0 )
		ensureChips ( size_t ( count ) );

	for ( auto index = 0; auto& c : chips )
		c->setVisible ( index++ < count );

	resized ();
}
//-----------------------------------------------------------------------------

void GUI_FrequencyLines::reset ( const bool isNTSC )
{
	for ( auto& c : chips )
		c->reset ( isNTSC );
}
//-----------------------------------------------------------------------------

void GUI_FrequencyLines::updateState ( const int chipNo, uint8_t* regs, const int regIndex )
{
	chipAt ( size_t ( chipNo ) ).updateState ( regs, regIndex );
}
//-----------------------------------------------------------------------------
