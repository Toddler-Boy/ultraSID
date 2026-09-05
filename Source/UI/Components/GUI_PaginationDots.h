#pragma once

#include <JuceHeader.h>

class GUI_PaginationDots final : public juce::Component
{
public:
	GUI_PaginationDots ();

	// juce::Component
	void paint ( juce::Graphics& g ) override;

	void mouseEnter ( const juce::MouseEvent& evt ) override;
	void mouseMove ( const juce::MouseEvent& evt ) override;
	void mouseExit ( const juce::MouseEvent& evt ) override;

	void mouseDown ( const juce::MouseEvent& evt ) override;
	void mouseDrag ( const juce::MouseEvent& evt ) override;

	// this
	void setNumberOfPages ( const int numPages );
	void setCurrentPage ( const int page, const bool notifiy = false );
	[[ nodiscard ]] int getCurrentPage () const { return currentPage; }
	void updateLayout ();

private:
	[[ nodiscard ]] int	positionToPageNo ( const int x ) const;

	int		numberOfPages = 0;
	int		currentPage = -1;

	int		visiblePoints = 0;
	int		hoverPosition = -1;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_PaginationDots )
};
//-----------------------------------------------------------------------------
