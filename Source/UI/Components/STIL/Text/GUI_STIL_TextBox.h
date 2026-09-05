#pragma once

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

#include "GUI_STIL_Item.h"
#include "GUI_STIL_Portrait.h"
#include "GUI_STIL_TextArea.h"

//----------------------------------------------------------------------------------

// The colored marker line at the edge of a STIL box
class GUI_STIL_Marker final : public juce::Component
{
public:
	GUI_STIL_Marker ( const int _colorId )
		: colorId ( _colorId )
	{
		setName ( "marker" );
		setInterceptsMouseClicks ( false, false );
	}

	void paint ( juce::Graphics& g ) override
	{
		const auto	b = getLocalBounds ().toFloat ();

		g.setColour ( findColour ( colorId ) );
		g.fillRoundedRectangle ( b, b.getWidth () / 2.0f );
	}

private:
	int	colorId;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_Marker )
};
//----------------------------------------------------------------------------------

// Shared shell of all STIL text boxes: paints the background box and hosts
// the marker line, the text area and an optional author strip (portrait +
// name), all positioned by the layout json
class GUI_STIL_TextBox : public GUI_STIL_Item
{
public:
	void layout ( const int width ) final
	{
		// The themed spacings enter the layout as constants, rounded to the
		// integer grid gin works on
		const auto	pad = UI::paddingDef ( UI::paddings::stil_box );
		const auto	markerPad = UI::paddingDef ( UI::paddings::stil_marker );

		layoutSupport.setConstant ( "padTop", juce::roundToInt ( pad.top ) );
		layoutSupport.setConstant ( "padRight", juce::roundToInt ( pad.right ) );
		layoutSupport.setConstant ( "padBottom", juce::roundToInt ( pad.bottom ) );
		layoutSupport.setConstant ( "padLeft", juce::roundToInt ( pad.left ) );

		layoutSupport.setConstant ( "markerLine", juce::roundToInt ( UI::lineWidth ( UI::lines::stil_marker ) ) );
		layoutSupport.setConstant ( "markerLeft", juce::roundToInt ( markerPad.left ) );
		layoutSupport.setConstant ( "markerTop", juce::roundToInt ( markerPad.top ) );
		layoutSupport.setConstant ( "markerBottom", juce::roundToInt ( markerPad.bottom ) );
		layoutSupport.setConstant ( "markerRight", juce::roundToInt ( markerPad.right ) );

		const auto	authorPad = UI::paddingDef ( UI::paddings::stil_author );

		layoutSupport.setConstant ( "authorPadTop", juce::roundToInt ( authorPad.top ) );
		layoutSupport.setConstant ( "authorPadRight", juce::roundToInt ( authorPad.right ) );
		layoutSupport.setConstant ( "authorPadBottom", juce::roundToInt ( authorPad.bottom ) );
		layoutSupport.setConstant ( "authorPadLeft", juce::roundToInt ( authorPad.left ) );
		layoutSupport.setConstant ( "authorHeight", juce::roundToInt ( UI::paddingDef ( UI::paddings::stil_author_height ).top ) );

		layoutSupport.setConstant ( "itemWidth", width );
		layoutSupport.setConstant ( "hasAuthor", portrait ? 1 : 0 );

		// The first pass only resolves the text area width, the second pass
		// applies the measured text height
		layoutSupport.setConstant ( "textHeight", 0 );
		runLayout ();

		layoutSupport.setConstant ( "textHeight", layoutText ( textArea.getWidth () ) );
		runLayout ();

		// Re-resolved here so theme switches land (they re-flow the view)
		if ( authorName )
			authorName->setColour ( juce::Label::textColourId, textColor () );

		textLaidOut ();
	}

	int gapBelow () const override	{	return juce::roundToInt ( UI::paddingDef ( UI::paddings::stil_box_gap ).top );	}

	void paint ( juce::Graphics& g ) override
	{
		const auto	b = getLocalBounds ().toFloat ();

		g.setColour ( findColour ( boxColorId ).withMultipliedAlpha ( UI::paddingDef ( UI::paddings::stil_box_alpha ).top ) );
		g.fillRoundedRectangle ( b, UI::corner ( UI::corners::stil_box, b ) );

		// The link hover highlight paints here so its expansion isn't clipped
		// at the text area's bounds; the text itself still lands on top
		if ( auto hover = textArea.hoverHighlight (); ! hover.isEmpty () )
		{
			hover.applyTransform ( juce::AffineTransform::translation ( float ( textArea.getX () ), float ( textArea.getY () ) ) );

			g.setColour ( findColour ( UI::colors::stilLink ).withMultipliedBrightness ( 0.3f ) );
			g.fillPath ( hover );
		}
	}

	// The box's content color, shared by every box so text and icons stay
	// visually consistent
	[[ nodiscard ]] juce::Colour textColor () const
	{
		return findColour ( boxColorId );
	}

protected:
	GUI_STIL_TextBox ( const int _boxColorId, juce::String text, const UI::fonts::Role _textFont )
		: rawText ( std::move ( text ) )
		, marker ( _boxColorId )
		, boxColorId ( _boxColorId )
		, textFont ( _textFont )
	{
		addAndMakeVisible ( textArea );
		addAndMakeVisible ( marker );
	}

	// Creates the text block for the given width and returns its height
	virtual int layoutText ( const int width )
	{
		textArea.setTextBlock ( rawText, width, textFont, textColor () );

		return textArea.textHeight ();
	}

	// Called after the final layout pass, for content the layout file doesn't
	// place itself
	virtual void textLaidOut ()	{}

	// A top-level constant of the layout files, valid once layout () ran
	[[ nodiscard ]] int layoutConstant ( const juce::String& name, const int defaultValue ) const
	{
		return int ( layoutSupport.getConstant ( name, defaultValue ) );
	}

	void setAuthor ( const juce::String& name, juce::Image image, const bool portraitIconFallback )
	{
		authorImage = image;

		portrait = std::make_unique<GUI_STIL_Portrait> ( portraitIconFallback );
		portrait->getImage = [ this ] { return authorImage; };
		portrait->getTint = [ this ] { return textColor (); };

		authorName = std::make_unique<juce::Label> ( "author", name );
		authorName->setBorderSize ( {} );
		authorName->setInterceptsMouseClicks ( false, false );
		UI::setFontRole ( *authorName, UI::fonts::stil_author );

		addAndMakeVisible ( *portrait );
		addAndMakeVisible ( *authorName );
	}

	juce::String		rawText;

	GUI_STIL_TextArea	textArea;
	GUI_STIL_Marker		marker;

private:
	void runLayout ()
	{
		UI::setLayout ( layoutSupport, {	"UI/layouts/constants.json",
											"UI/layouts/components/stil-box.json" } );
	}

	int				boxColorId;
	UI::fonts::Role	textFont;
	juce::Image		authorImage;

	std::unique_ptr<GUI_STIL_Portrait>	portrait;
	std::unique_ptr<juce::Label>		authorName;

	gin::LayoutSupport	layoutSupport { *this };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( GUI_STIL_TextBox )
};
//----------------------------------------------------------------------------------
