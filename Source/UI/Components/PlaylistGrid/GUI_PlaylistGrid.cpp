#include "GUI_PlaylistGrid.h"

#include "libSidplayEZ/src/stringutils.h"

#include "ultra-shared/Helpers/Regex.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Helpers/Messages.h"
#include "UI/Pages/GUI_Pages.h"

//-----------------------------------------------------------------------------

constexpr auto	bentoGap = 8;

//-----------------------------------------------------------------------------

GUI_PlaylistGrid::GUI_PlaylistGrid ( GUI_Pages& _pages, const bool _mini )
	: pages ( _pages )
	, mini ( _mini )
{
	setName ( "playlistGrid" );

	if ( mini )
		header.setFontRole ( UI::fonts::grid_mini_header );

	header.setName ( "header" );
	addAndMakeVisible ( header );

	if ( mini )
	{
		addAndMakeVisible ( headerButton );

		headerButton.setTooltip ( "playlist/add_playlist" );
		headerButton.alpha[ 0 ] = 0.33f;

		headerButton.onClick = []
		{
			const juce::SharedResourcePointer<Playlists>	playlists;

			auto	newName = playlists->addToPlaylist ( "", {} );
			msg::PlaylistNew { newName }.send ();
			msg::ShowPlaylist { newName }.send ();
		};
	}

	viewport.setName ( "viewport" );
	viewport.setScrollBarsShown ( true, false );

	// No tab stop; the items' keys are handled by itemKeys
	viewport.setWantsKeyboardFocus ( false );
	viewport.setViewedComponent ( &grid, false );
	addAndMakeVisible ( viewport );

	grid.setName ( "items" );
	grid.addKeyListener ( &itemKeys );
}
//-----------------------------------------------------------------------------

GUI_PlaylistGrid::~GUI_PlaylistGrid ()
{
	grid.removeKeyListener ( &itemKeys );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::resized ()
{
	const auto	vpY = header.getBottom () + bentoGap;

	if ( mini )
		viewport.setBounds ( bentoGap, vpY, getWidth () - bentoGap * 0.5, getHeight () - vpY );
	else
		viewport.setBounds ( bentoGap, vpY, getWidth () - bentoGap * 1.5, getHeight () - vpY );

	layout ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::setPlaylists ( const juce::StringArray& list )
{
	grid.removeAllChildren ();
	items.clear ();

	for ( const auto& entry : list )
		addPlaylist ( entry, false );

	setCursor ( cursor );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::addPlaylist ( const juce::String& name, const bool withSort/* = true*/)
{
	auto	item = new GUI_PlaylistGridItem ( pages, name, mini );
	if ( withSort )
		items.addSorted ( *item, item );
	else
		items.add ( item );

	item->onClick = [ item, this ]
	{
		setCursor ( items.indexOf ( item ) );

		// Prevent unselected buttons from firing
		if ( item->isToggleable () && ! item->getToggleState () )
			return;

		msg::ShowPlaylist { item->getName () }.send ();
	};

	item->onFocus = [ item, this ] ( const FocusChangeType cause )
	{
		itemFocused ( *item, cause );
	};

	grid.addAndMakeVisible ( item );

	layout ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::removePlaylist ( const juce::String& name )
{
	for ( auto item : items )
	{
		if ( item->getName () == name )
		{
			grid.removeChildComponent ( item );
			items.removeObject ( item );
			break;
		}
	}

	layout ();
	setCursor ( cursor );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::selectPlaylist ( const juce::String& name )
{
	for ( auto item : items )
	{
		if ( item->getName () == name )
		{
			item->setToggleState ( true, juce::dontSendNotification );
			setCursor ( items.indexOf ( item ) );
			break;
		}
	}
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::scrollToItem ( const GUI_PlaylistGridItem& item )
{
	// Scroll to item if it's not fully visible
	const auto	itemY = item.getY ();
	const auto	viewY = viewport.getViewPositionY ();
	const auto	viewHeight = viewport.getHeight ();

	if ( itemY < viewY )
		viewport.setViewPosition ( 0, itemY );
	else if ( item.getBottom () > viewY + viewHeight )
		viewport.setViewPosition ( 0, item.getBottom () - viewHeight );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::setCursor ( const int index )
{
	if ( items.isEmpty () )
	{
		cursor = 0;
		return;
	}

	cursor = juce::jlimit ( 0, items.size () - 1, index );
	scrollToItem ( *items[ cursor ] );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::itemFocused ( GUI_PlaylistGridItem& item, const FocusChangeType cause )
{
	const auto	index = items.indexOf ( &item );

	// A click makes the clicked item the cursor
	if ( cause == focusChangedByMouseClick )
	{
		setCursor ( index );
		return;
	}

	// Keyboard entry lands on the cursor item
	if ( juce::isPositiveAndBelow ( cursor, items.size () ) && index != cursor )
	{
		items[ cursor ]->grabKeyboardFocus ();
		return;
	}

	// Cursor item into view
	setCursor ( index );
}
//-----------------------------------------------------------------------------

bool GUI_PlaylistGrid::navigate ( const juce::KeyPress& key )
{
	// Tab leaves the grid
	if ( key.isKeyCode ( juce::KeyPress::tabKey ) && ! items.isEmpty () )
	{
		const auto	forwards = ! key.getModifiers ().isShiftDown ();

		( forwards ? items.getLast () : items.getFirst () )->moveKeyboardFocusToSibling ( forwards );
		return true;
	}

	// Modified keys belong to the shortcuts
	if ( key.getModifiers ().isAnyModifierKeyDown () || items.isEmpty () )
		return false;

	// The focused item is the cursor
	for ( auto i = 0; auto item : items )
	{
		if ( item->hasKeyboardFocus ( false ) )
			cursor = i;

		++i;
	}

	const auto	count = items.size ();
	auto		target = cursor;

	// A page is the number of rows the viewport shows
	const auto	rowPitch = itemsPerRow < count ? items[ itemsPerRow ]->getY () - items[ 0 ]->getY () : items[ 0 ]->getHeight ();
	const auto	pageRows = std::max ( 1, viewport.getHeight () / std::max ( 1, rowPitch ) );

	if ( key.isKeyCode ( juce::KeyPress::homeKey ) )
		target = 0;
	else if ( key.isKeyCode ( juce::KeyPress::endKey ) )
		target = count - 1;
	else if ( key.isKeyCode ( juce::KeyPress::pageUpKey ) )
		target = std::max ( 0, cursor - pageRows * itemsPerRow );
	else if ( key.isKeyCode ( juce::KeyPress::pageDownKey ) )
		target = std::min ( count - 1, cursor + pageRows * itemsPerRow );
	else if ( key.isKeyCode ( juce::KeyPress::upKey ) )
		target -= itemsPerRow;
	else if ( key.isKeyCode ( juce::KeyPress::downKey ) )
	{
		target += itemsPerRow;

		// Down from a full row onto a shorter last row lands on its last item
		if ( target >= count && cursor / itemsPerRow < ( count - 1 ) / itemsPerRow )
			target = count - 1;
	}
	else if ( ! mini && key.isKeyCode ( juce::KeyPress::leftKey ) )
		--target;
	else if ( ! mini && key.isKeyCode ( juce::KeyPress::rightKey ) )
		++target;
	else
		return false;

	// Edges consume the key
	if ( juce::isPositiveAndBelow ( target, count ) )
	{
		setCursor ( target );
		items[ target ]->grabKeyboardFocus ();
	}

	return true;
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::updateGridItem ( GUI_PlaylistGridItem& item )
{
	// Add database-entries to grid-item
	juce::StringArray	authors;

	const auto&	name = item.getName ().toStdString ();

	const regex::Pattern	re ( R"(\s*\([^\)]*\))" );

	const auto&	playlistEntries = pages.getPlaylistEntries ( name );

	for ( const auto entry : playlistEntries )
	{
		if ( ! entry )	// Missing tune, no author to credit
			continue;

		auto	str = stringutils::extendedASCIItoUTF8 ( entry->author );

		// Remove nicknames in brackets, e.g. "Artist Name (Nickname)" -> "Artist Name"
		str = re.replaceAll ( str, "" );

		// "Tim & Geoff Follin" are credited together, split them into two authors
		if ( str == "Tim & Geoff Follin" )
			str = "Tim Follin & Geoff Follin";

		// Split multiple authors separated by "&", e.g. "Artist 1 & Artist 2" -> "Artist 1", "Artist 2"
		auto	split = juce::StringArray::fromTokens ( str, "&,", "" );

		split.trim ();
		split.removeEmptyStrings ();

		for ( auto s : split )
		{
			const auto	normalized = s.replace ( " ", "" );

			// Expand some common abbreviations, e.g. "J. Tel" -> "Jeroen Tel"
			if ( normalized == "J.Tel" )			s = "Jeroen Tel";
			else if ( normalized == "G.Tjelta" )	s = "Geir Tjelta";

			authors.addIfNotAlreadyThere ( s, true );
		}
	}

	item.setBasicInfo ( juce::String ( playlistEntries.size () ) + " Tunes" );
	item.setTuneCount ( int ( playlistEntries.size () ) );
	item.setAuthors ( authors );
	item.lookAndFeelChanged ();

	if ( auto pl = pages.getPlaylistItems ( name ) )
	{
		if ( auto img = pl->getCoverImage (); img.isValid () )
			item.setImage ( img );
		else
			item.setImages ( pages.getNonEmptyThumbnails ( name ) );
	}

	item.repaint ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::updateContent ()
{
	for ( auto item : items )
		updateGridItem ( *item );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::updateGridItemByName ( const juce::String& name )
{
	for ( auto item : items )
	{
		if ( item->getName () == name )
		{
			updateGridItem ( *item );
			break;
		}
	}
}
//-----------------------------------------------------------------------------

void GUI_PlaylistGrid::layout ()
{
	if ( items.isEmpty () )
	{
		grid.setSize ( 0, 0 );
		return;
	}

	if ( mini )
	{
		const auto	width = viewport.getWidth () - viewport.getScrollBarThickness ();
		constexpr auto	itemHeight = 52;

		itemsPerRow = 1;

		auto	y = 0;
		for ( auto item : items )
		{
			item->setBounds ( 0, y, width, itemHeight );
			y += itemHeight;
		}

		grid.setSize ( width, items.getLast ()->getBottom () );
	}
	else
	{
		constexpr auto	maxItemWidth = 176.0f;
		constexpr auto	itemScale = 0.85f;
		constexpr auto	gapX = 16;
		constexpr auto	gapY = 16;

		const auto	width = viewport.getWidth () - viewport.getScrollBarThickness () - bentoGap * 2;
		const auto	fWidth = float ( width );

		if ( fWidth < maxItemWidth )
			return;

		itemsPerRow = std::max ( 1, std::min ( items.size (), int ( fWidth / ( maxItemWidth * itemScale + gapX ) ) ) );
		const auto	fItemWidth = std::min ( maxItemWidth / itemScale, ( fWidth - itemsPerRow * gapX ) / float ( itemsPerRow ) );

		itemWidth = int ( fItemWidth );
		itemHeight = int ( fItemWidth * 1.38f );

		auto	x = bentoGap * 2;
		auto	y = bentoGap * 2;
		for ( auto item : items )
		{
			item->setBounds ( x, y, itemWidth, itemHeight );
			x += itemWidth + int ( gapX );
			if ( ( x + itemWidth ) > width )
			{
				x = bentoGap * 2;
				y += itemHeight + gapY;
			}
		}

		grid.setSize ( width + bentoGap * 2, items.getLast ()->getBottom () + bentoGap * 2 );
	}
}
//-----------------------------------------------------------------------------

GUI_AutoScrollContainer::GUI_AutoScrollContainer ()
{
	startTimerHz ( 60 );
}
//-----------------------------------------------------------------------------

GUI_AutoScrollContainer::~GUI_AutoScrollContainer ()
{
	stopTimer ();
}
//-----------------------------------------------------------------------------

void GUI_AutoScrollContainer::timerCallback ()
{
	if ( getNumChildComponents () == 0 )
		return;

	const auto	child = getChildComponent ( 0 );
	if ( const auto	dragContainer = juce::DragAndDropContainer::findParentDragContainerFor ( child ); ! dragContainer || ! dragContainer->isDragAndDropActive () )
		return;

	if ( auto* viewport = findParentComponentOfClass<juce::Viewport> () )
	{
		auto		mouseSource = juce::Desktop::getInstance ().getMainMouseSource ();
		const auto	globalMousePos = mouseSource.getScreenPosition ().toInt ();
		const auto	mouseInViewportSpace = viewport->getLocalPoint ( nullptr, globalMousePos );

		if ( mouseInViewportSpace.x < 0 || mouseInViewportSpace.x >= viewport->getWidth () )
			return;

		constexpr auto	edgeZone = 40;
		const auto	maxSpeed = child->getHeight () / 4;

		viewport->autoScroll ( mouseInViewportSpace.x, mouseInViewportSpace.y, edgeZone, maxSpeed );

		mouseSource.triggerFakeMove ();
	}
}
//-----------------------------------------------------------------------------
