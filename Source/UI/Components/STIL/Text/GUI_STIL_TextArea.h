#pragma once

#include <JuceHeader.h>

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//----------------------------------------------------------------------------------

// The text block of a STIL box: renders the laid-out text and handles the
// embedded tune links (hover, click, drag). The owner's layout json positions
// it, so the link hit-boxes live in local coordinates
class GUI_STIL_TextArea final : public juce::Component
{
public:
	GUI_STIL_TextArea ()
	{
		setName ( "textArea" );
		setInterceptsMouseClicks ( false, false );
	}

	// Link-aware STIL text
	void setTextBlock ( const juce::String& rawText, const int width, const UI::fonts::Role fontRole, const juce::Colour color );

	// Pre-built text without link detection (mono comments)
	void setBlock ( const juce::AttributedString& as, const int width );

	[[ nodiscard ]] int textHeight () const	{	return int ( std::ceil ( textLayout.getHeight () ) );	}

	// The hovered link as a rounded path in local coordinates; the owning box
	// paints it, its bounds have the room the expansion needs
	[[ nodiscard ]] juce::Path hoverHighlight () const;

	void paint ( juce::Graphics& g ) override;

	void mouseEnter ( const juce::MouseEvent& e ) override	{	mouseMove ( e );	}
	void mouseMove ( const juce::MouseEvent& e ) override;
	void mouseExit ( const juce::MouseEvent& e ) override;

	void mouseUp ( const juce::MouseEvent& e ) override;
	void mouseDrag ( const juce::MouseEvent& e ) override;

private:
	struct Link
	{
		std::string					link;
		juce::RectangleList<float>	bounds;
		juce::Range<int>			range;
	};

	[[ nodiscard ]] Link* getLink ( juce::Point<float> mouse );

	void repaintHighlight ();

	// The link font derives from the block's font role, kept for the drag image
	[[ nodiscard ]] juce::Font linkFont () const	{	return UI::fontSized ( UI::fontDef ( fontRole ).size, 600 );	}

	juce::TextLayout			textLayout;
	std::vector<Link>			links;
	Link*						link = nullptr;

	UI::fonts::Role				fontRole {};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_TextArea )
};
//----------------------------------------------------------------------------------
