#include "GUI_ultraSID_logo.h"

#include "ultra-shared/UI/UI_Helpers.h"

#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

constexpr auto	hoverHueShift = -20.0f / 360.0f;

//-----------------------------------------------------------------------------

GUI_ultraSID_logo::GUI_ultraSID_logo ( const juce::String& name, const juce::String& resource )
	: juce::Button ( name )
{
	setMouseCursor ( juce::MouseCursor::PointingHandCursor );

	auto	[ logo, _ ] = UI::getSVG ( resource );

    if ( logo )
        collectPaths ( *logo, {} );
}
//-----------------------------------------------------------------------------

void GUI_ultraSID_logo::paintButton ( juce::Graphics& g, const bool isHover, bool )
{
	const auto	fit = juce::RectanglePlacement ( juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yTop )
											.getTransformToFit ( logoBounds, getLocalBounds ().toFloat () );

	for ( const auto& p : paths )
	{
		auto	col = findColour ( p.roleId );

		if ( isHover )
			col = col.withRotatedHue ( hoverHueShift );

		g.setColour ( col );
		g.fillPath ( p.path, fit );
	}
}
//-----------------------------------------------------------------------------

void GUI_ultraSID_logo::collectPaths ( const juce::Drawable& d, const juce::AffineTransform& parentTransform )
{
	if ( auto* shape = dynamic_cast<const juce::DrawableShape*> ( &d ) )
	{
		auto	p = shape->getOutlineAsPath ();
		p.applyTransform ( parentTransform );

		// The SVG's fill colors are role markers: red picks the outline role, everything else the logo fill
		const auto&	fill = shape->getFill ();
		const auto	outline = fill.isColour () && fill.colour == juce::Colours::red;

		logoBounds = logoBounds.getUnion ( p.getBounds () );

		paths.push_back ( { outline ? UI::colors::logoOutline : UI::colors::logo, std::move ( p ) } );
		return;
	}

	if ( auto* composite = dynamic_cast<const juce::DrawableComposite*> ( &d ) )
	{
		auto	t = composite->getDrawableTransform ().followedBy ( parentTransform );

		for ( auto i = 0; i < composite->getNumChildren (); ++i )
			collectPaths ( composite->getChild ( i ), t );
	}
}
//-----------------------------------------------------------------------------
