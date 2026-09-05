#include "GUI_PlaylistGridItem.h"

#include "ultra-shared/Helpers/TextUtils.h"
#include "ultra-shared/Resources/Theme.h"
#include "ultra-shared/UI/GUI_LookAndFeel.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Helpers/Messages.h"
#include "UI/Pages/GUI_Pages.h"
#include "UI/ui-colors.h"
#include "UI/UI_Menus.h"

//-----------------------------------------------------------------------------

GUI_PlaylistGridItem::GUI_PlaylistGridItem ( GUI_Pages& _pages, const juce::String& name, const bool _mini )
	: juce::Button ( name )
	, pages ( _pages )
	, mini ( _mini )
{
	if ( mini )
		setRadioGroupId ( 1, juce::dontSendNotification );

	setToggleable ( mini );
	setClickingTogglesState ( mini );

	// The play overlay is no tab stop
	playButton.setWantsKeyboardFocus ( false );

	// Focus ring concentric to the card's corner
	getProperties ().set ( "focusRadius", UI::corners::name ( mini ? UI::corners::grid_mini : UI::corners::grid_big ) );

	playButton.setInterceptsMouseClicks ( false, false );
	playButton.margin = mini * 5.0f;

	addAndMakeVisible ( coverDisplay );
	addChildComponent ( playButton );

	coverDisplay.addChangeListener ( this );
}
//-----------------------------------------------------------------------------

GUI_PlaylistGridItem::~GUI_PlaylistGridItem ()
{
	coverDisplay.removeChangeListener ( this );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::resized ()
{
	if ( mini )
	{
		const auto	b = getLocalBounds ().reduced ( 8 );

		coverDisplay.setBounds ( b.withWidth ( b.getHeight () ) );
		playButton.setBounds ( coverDisplay.getBounds () );
	}
	else
	{
		auto	b = getLocalBounds ().reduced ( 16 );

		coverDisplay.setBounds ( b.withHeight ( b.getWidth () ) );

		const auto	playSize = b.getWidth () / 2;

		b = juce::Rectangle<int> ( b.getX (), b.getY () + playSize, playSize, playSize );
		playButton.setBounds ( b.reduced ( playSize / 5 ) );
	}
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::mouseEnter ( const juce::MouseEvent& e )
{
	juce::Button::mouseEnter ( e );

	if ( isDragging )
		return;

	if ( ! mini || playButton.getBoundsInParent ().contains ( e.x, e.y ) )
		startHoverAnim ( false );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::mouseExit ( const juce::MouseEvent& e )
{
	juce::Button::mouseExit ( e );

	if ( isDragging )
		return;

	stopHoverAnim ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::mouseMove ( const juce::MouseEvent& e )
{
	juce::Button::mouseMove ( e );

	if ( ! mini || isDragging )
		return;

	if ( ! playButton.getBoundsInParent ().contains ( e.x, e.y ) )
		stopHoverAnim ();
	else
		startHoverAnim ( false );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::lookAndFeelChanged ()
{
	playButton.lookAndFeelChanged ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::focusGained ( FocusChangeType cause )
{
	juce::Button::focusGained ( cause );

	if ( onFocus )
		onFocus ( cause );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::paintButton ( juce::Graphics& g, bool isHover, bool /*isDown*/ )
{
	auto drawDragging = [ &g, this ] ( const float radius )
	{
		if ( ! isDragging )
			return;

		g.setColour ( UI::getShade ( 1.0f ) );

		GUI_LookAndFeel::drawOutline ( g, getLocalBounds ().toFloat (), radius, UI::lineWidth ( UI::lines::grid_drag ) );
	};

	if ( mini )
	{
		auto	b = getLocalBounds ().toFloat ();

		if ( getToggleState () )
		{
			const auto	bgCol = UI::getShade ( UI::shades::selected );
			const auto	fgCol = findColour ( UI::colors::text );

			g.setColour ( bgCol );
			GUI_LookAndFeel::drawOutlinedRect ( g, b, UI::corner ( UI::corners::grid_mini, b ), UI::lineWidth ( UI::lines::grid_outline ), bgCol.interpolatedWith ( fgCol, 0.1f ) );
		}
		else if ( isHover )
		{
			g.setColour ( UI::getShade ( UI::shades::hover ) );
			g.fillRoundedRectangle ( b, UI::corner ( UI::corners::grid_mini, b ) );
		}

		b = UI::padded ( b, UI::paddings::grid_mini );
		b.removeFromLeft ( b.getHeight () + 12.0f );

		//
		// Name
		//
		g.setFont ( UI::font ( UI::fonts::grid_mini_name ) );
		g.setColour ( findColour ( UI::colors::text ) );
		g.drawText ( getButtonText (), b.removeFromTop ( b.getHeight () / 2.0f ), juce::Justification::centredLeft );

		g.setFont ( UI::font ( UI::fonts::grid_mini_info ) );
		g.setColour ( findColour ( UI::colors::textMuted ) );
		g.drawText ( info, b, juce::Justification::centredLeft );

		drawDragging ( UI::corner ( UI::corners::grid_mini, getLocalBounds ().toFloat () ) );
	}
	else
	{
		auto	b = getLocalBounds ().toFloat ();

		const auto	bgCol = UI::getShade ( 0.1f ).interpolatedWith ( cardCol, ease );
		const auto	fgCol = findColour ( UI::colors::text );
		const auto	txtCol = fgCol.interpolatedWith ( cardTextCol, ease );

		g.setColour ( bgCol );
		GUI_LookAndFeel::drawOutlinedRect ( g, b, UI::corner ( UI::corners::grid_big, b ), UI::lineWidth ( UI::lines::grid_outline ), bgCol.interpolatedWith ( fgCol, 0.1f ) );

		b.reduce ( 16.0f, 0.0f );
		b.removeFromTop ( 16.0f + b.getWidth () + 8.0f );

		//
		// Name
		//
		g.setFont ( UI::font ( UI::fonts::grid_big_title ) );
		g.setColour ( txtCol );
		g.drawText ( getButtonText (), b.removeFromTop ( 25.0f ), juce::Justification::topLeft);

		//
		// Authors
		//
		b.removeFromBottom ( 8.0f );

		g.setFont ( UI::font ( UI::fonts::grid_big_authors ) );
		g.setColour ( txtCol.withMultipliedAlpha ( 0.5f ) );
		g.drawFittedText ( authors, b.getSmallestIntegerContainer (), juce::Justification::topLeft, 3, 1.0f );

		drawDragging ( UI::corner ( UI::corners::grid_big, getLocalBounds ().toFloat () ) );
	}
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::mouseDown ( const juce::MouseEvent& e )
{
	if ( ! e.mods.isPopupMenu () )
	{
		if ( e.mods.isLeftButtonDown () && playButton.getBoundsInParent ().contains ( e.x, e.y ) )
		{
			msg::PlayPlaylist { getName () }.send ();
			return;
		}

		juce::Button::mouseDown ( e );
		return;
	}

	auto	m = UI::newPopupMenu ( *this );

	UI::menu_ExportPlaylist ( m, getName () );
	m.addSeparator ();

	// Delete cover image
	UI::menu_DeleteCover ( m, getName () );
	m.addSeparator ();
	UI::menu_DeletePlaylist ( m, getName () );

	UI::showMenuAtMouse ( m, *this );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::mouseUp ( const juce::MouseEvent& e )
{
	if ( e.mods.isPopupMenu () )
		return;

	if ( playButton.getBoundsInParent ().contains ( e.x, e.y ) )
		return;

	juce::Button::mouseUp ( e );
}
//-----------------------------------------------------------------------------

bool GUI_PlaylistGridItem::isInterestedInDragSource ( const SourceDetails& dragSourceDetails )
{
	// The card is a proxy for its playlist, so it accepts exactly what the list accepts
	if ( auto items = pages.getPlaylistItems ( getName () ) )
		return items->isInterestedInDragSource ( dragSourceDetails );

	return false;
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::itemDropped ( const SourceDetails& dragSourceDetails )
{
	if ( auto items = pages.getPlaylistItems ( getName () ) )
		items->itemDropped ( dragSourceDetails );

	isDragging = false;
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::changeListenerCallback ( juce::ChangeBroadcaster* /*source*/ )
{
	const auto	col = coverDisplay.getAverageColor ();

	cardCol = UI::getColorWithPerceivedBrightness ( col, 0.1f );
	cardTextCol = UI::getColorWithPerceivedBrightness ( col, 0.8f );

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::timerCallback ()
{
	// Animate
	{
		constexpr auto	animSpeed = 1.0f / ( 100 / ( 1000.0f / 60.0f ) );

		if ( animTarget > animCur )
			animCur = std::min ( animCur + animSpeed, animTarget );
		else if ( animTarget < animCur )
			animCur = std::max ( animCur - animSpeed * 0.25f, animTarget );
	}

	ease = UI::easeInOutQuad ( animCur );

	// Play button
	playButton.setAlpha ( ease );
	playButton.setVisible ( ease > 0.0f && showPlayButton );

	// Cover hover
	coverDisplay.setHoverBlend ( ease );

	if ( juce::approximatelyEqual ( animCur, animTarget ) )
	{
		stopTimer ();

		if ( ease <= 1e-6f && showPlayButton )
			showPlayButton = false;
	}

	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::startHoverAnim ( const bool dragging )
{
	isDragging = dragging;
	showPlayButton = ! dragging;

	setMouseCursor ( dragging ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor );

	animTarget = 1.0f;
	if ( ! juce::approximatelyEqual ( animCur, animTarget ) )
		startTimerHz ( 60 );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::stopHoverAnim ()
{
	isDragging = false;

	setMouseCursor ( juce::MouseCursor::NormalCursor );

	animTarget = 0.0f;
	if ( ! juce::approximatelyEqual ( animCur, animTarget ) )
		startTimerHz ( 60 );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::setImages ( const std::vector<const Database::entry*>& mips )
{
	coverDisplay.setImages ( mips );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::setImage ( juce::Image& image )
{
	coverDisplay.setImage ( image );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::setAuthors ( const juce::StringArray& _authors )
{
	authors = _authors.joinIntoString ( ", " );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::setTuneCount ( const int count )
{
	const auto	previous = std::exchange ( tuneCount, count );

	if ( previous < 0 || count == previous || ! isShowing () )
		return;

	// Mini rows have free space on the right, cards use the cover corner
	const auto	local = mini ? juce::Point<int> ( getWidth () - 24, getHeight () / 2 )
							 : coverDisplay.getBounds ().getTopRight ().translated ( -8, 16 );

	const auto	screen = localPointToGlobal ( local );
	msg::BadgeSpawn { screen.x, screen.y, count - previous }.send ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::setBasicInfo ( const juce::String& _info )
{
	info = _info;
}
//-----------------------------------------------------------------------------

bool GUI_PlaylistGridItem::isInterestedInFileDrag ( const juce::StringArray& files )
{
	return textutils::getFilteredStrings ( files, { ".png", ".jpg" } ).size ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::filesDropped ( const juce::StringArray& files, int /*x*/, int /*y*/ )
{
	const juce::SharedResourcePointer<Playlists>	pl;

	pl->applyCoverDrop ( getName (), files );

	isDragging = false;
	repaint ();
}
//-----------------------------------------------------------------------------

bool GUI_PlaylistGridItem::isInterestedInTextDrag ( const juce::String& text )
{
	return textutils::isUrlWithExtension ( text, { ".png", ".jpg" } );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGridItem::textDropped ( const juce::String& text, int /*x*/, int /*y*/ )
{
	msg::DownloadCover { getName (), text.trim () }.send ();

	isDragging = false;
	repaint ();
}
//-----------------------------------------------------------------------------
