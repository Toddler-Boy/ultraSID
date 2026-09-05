#include "GUI_PaginationDots.h"

#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

static constexpr auto	dotSize = 7;

//-----------------------------------------------------------------------------

GUI_PaginationDots::GUI_PaginationDots ()
{
	setName ( "pageControl" );
	setBufferedToImage ( true );
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::paint ( juce::Graphics& g )
{
	if ( numberOfPages < 2 )
		return;

	auto	bounds = getLocalBounds ().toFloat ();
	if ( bounds.isEmpty () )
		return;

	// Draw the background
	g.setColour ( juce::Colours::black.withAlpha ( 0.5f ) );
	g.fillRoundedRectangle ( bounds, bounds.getHeight () * 0.5f );

	bounds.reduce ( dotSize, 0 );

	// Draw dots
	auto	pageWidth = bounds.getWidth () / visiblePoints;

	for ( auto i = 0; i < visiblePoints; ++i )
	{
		if ( i == currentPage )
			g.setColour ( juce::Colours::white );
		else if ( i == hoverPosition )
			g.setColour ( juce::Colours::white.withAlpha ( 0.66f ) );
		else
			g.setColour ( juce::Colours::white.withAlpha ( 0.33f ) );

		g.fillEllipse ( bounds.removeFromLeft ( pageWidth ).withSizeKeepingCentre ( dotSize, dotSize ) );
	}
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::mouseEnter ( const juce::MouseEvent& evt )
{
	mouseMove ( evt );
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::mouseMove ( const juce::MouseEvent& evt )
{
	const auto	pageNo = positionToPageNo ( evt.x );

	if ( pageNo == hoverPosition )
		return;

	hoverPosition = pageNo;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::mouseExit ( const juce::MouseEvent& /*evt*/ )
{
	hoverPosition = -1;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::mouseDown ( const juce::MouseEvent& evt )
{
	setCurrentPage ( positionToPageNo ( evt.x ), true );
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::mouseDrag ( const juce::MouseEvent& evt )
{
	hoverPosition = -1;
	mouseDown ( evt );
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::setNumberOfPages ( const int numPages )
{
	if ( numberOfPages == numPages )
		return;

	const auto	oldNumPages = numberOfPages;

	numberOfPages = numPages;
	updateLayout ();

	// If the current page is out of bounds, set it to the last page
	if ( oldNumPages > numberOfPages && currentPage >= numberOfPages )
		setCurrentPage ( numberOfPages - 1, true );
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::setCurrentPage ( const int page, const bool notifiy )
{
	if ( page >= numberOfPages )
		return;

	if ( currentPage == page )
		return;

	currentPage = page;

	if ( notifiy )
		msg::SetCRTPage { currentPage }.send ();

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_PaginationDots::updateLayout ()
{
	auto	p = getParentComponent ();
	if ( ! p )
		return;

	visiblePoints = std::clamp ( numberOfPages, 1, 50 );
	setVisible ( visiblePoints > 1 );

	if ( ! isVisible () )
		return;

	const auto	compWidth = int ( dotSize * 2.5 * visiblePoints + dotSize * 2 );
	const auto	pb = p->getLocalBounds ();

	setBounds ( pb.getCentreX () - compWidth / 2, pb.getBottom () - 40, compWidth, dotSize * 4 );

	repaint ();
}
//-----------------------------------------------------------------------------

int GUI_PaginationDots::positionToPageNo ( const int x ) const
{
	const auto	bounds = getLocalBounds ().reduced ( dotSize, 0 );

	return std::clamp ( int ( ( x - bounds.getX () ) / ( bounds.getWidth () / visiblePoints ) ), 0, visiblePoints - 1 );
}
//-----------------------------------------------------------------------------
