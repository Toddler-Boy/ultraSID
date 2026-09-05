#include "GUI_Chips.h"

//-----------------------------------------------------------------------------

GUI_Chips::GUI_Chips ()
{
	setName ( "chips" );

	setChipsUsed ( 1 );
	setModel ( 0, "6581" );
	setProfile ( 0, "", "", false );
}
//-----------------------------------------------------------------------------

void GUI_Chips::ensureChips ( size_t count )
{
	while ( chips.size () < count )
	{
		auto&	c = chips.emplace_back ( std::make_unique<GUI_Chip> () );

		c->setName ( "chip" + juce::String ( chips.size () ) );
		addChildComponent ( *c );
	}
}
//-----------------------------------------------------------------------------

GUI_Chip& GUI_Chips::chipAt ( size_t chipNo )
{
	ensureChips ( chipNo + 1 );

	return *chips[ chipNo ];
}
//-----------------------------------------------------------------------------

void GUI_Chips::resized ()
{
	if ( chipsUsed == 1 )
	{
		chipAt ( 0 ).setBounds ( getLocalBounds () );
		return;
	}

	// Two columns of half-size chips, filling left to right; an odd final chip
	// is centered (the classic 3-chip look is the two-row case of this rule).
	// Bounds stay at the artwork's design size (900 x 500 aspect), the 0.5
	// transform is what makes the panels half-size
	const auto	w = getWidth ();
	const auto	chipH = w * 500 / 900;

	constexpr auto	multiMargin = 10;

	for ( auto i = 0; i < int ( chips.size () ); ++i )
	{
		const auto	lastOdd = i == chipsUsed - 1 && ( chipsUsed & 1 );

		const auto	x = lastOdd ? w / 2 : ( i & 1 ) ? w - multiMargin : multiMargin;
		const auto	y = ( i / 2 ) * ( chipH - multiMargin );

		chips[ size_t ( i ) ]->setBounds ( x, y, w, chipH );
	}
}
//-----------------------------------------------------------------------------

void GUI_Chips::setChipsUsed ( int count )
{
	jassert ( count >= 1 );

	chipsUsed = count;

	ensureChips ( size_t ( count ) );

	// More than two chips scale down to fit the same space
	const auto	scl = juce::AffineTransform::scale ( chipsUsed >= 2 ? 0.5f : 1.0f );

	for ( auto index = 0; auto& c : chips )
	{
		c->setVisible ( index < chipsUsed );
		c->setTransform ( scl );

		++index;
	}

	resized ();
}
//-----------------------------------------------------------------------------

void GUI_Chips::setModel ( unsigned int chipNo, const std::string& model )
{
	chipAt ( chipNo ).setModel ( model );
}
//-----------------------------------------------------------------------------

void GUI_Chips::setDigiVisible ( const bool shouldBeVisible )
{
	for ( auto& c : chips )
		c->setDigiVisible ( shouldBeVisible );
}
//-----------------------------------------------------------------------------

void GUI_Chips::setProfile ( unsigned int chipNo, const std::string& chipProfile, const std::string& chipProfileBitmap, const bool goldenBorder )
{
	chipAt ( chipNo ).setProfile ( chipProfile, chipProfileBitmap, goldenBorder );
}
//-----------------------------------------------------------------------------
