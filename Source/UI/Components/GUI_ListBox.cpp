#include <JuceHeader.h>

#include "GUI_ListBox.h"

#include "libSidplayEZ/src/stringutils.h"

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/ThumbnailCache.h"
#include "Data/Likes.h"
#include "Data/Tags.h"
#include "Database/TuneInfo.h"
#include "Helpers/Messages.h"
#include "UI/GUI_AppLookAndFeel.h"
#include "UI/ui-colors.h"

//-----------------------------------------------------------------------------

GUI_ListBox::GUI_ListBox ()
	: juce::ComponentMovementWatcher ( this )
	, hover ( *this )
	, smoothScroll ( *this )
{
	hover.addChangeListener ( this );

	setModel ( this );

	setOutlineThickness ( 0 );
	setRowHeight ( 48 );
	setMultipleSelectionEnabled ( true );

	// Focus ring at twice the default margin
	getProperties ().set ( "focusMargin", "2" );

	setHeaderHeight ( 36 );
	getHeader ().setPopupMenuActive ( false );

	getViewport ()->setScrollBarsShown ( true, false );
}
//-----------------------------------------------------------------------------

GUI_ListBox::~GUI_ListBox ()
{
	hover.removeChangeListener ( this );
}
//-----------------------------------------------------------------------------

void GUI_ListBox::sortOrderChanged ( int newSortColumnId, bool isForwards )
{
	switch ( newSortColumnId )
	{
		case columnId::name:
			// Sort only by name
			std::ranges::sort ( rowData, [ isForwards ] ( const auto a, const auto b ) {
				const auto	cmp = lime::str::naturalCompare ( a->lowerName, b->lowerName );
				return isForwards ? cmp < 0 : cmp > 0;
			} );
			break;

		case columnId::release:
			// Sort by release first and name second
			std::ranges::sort ( rowData, [ isForwards ] ( const auto a, const auto b ) {

				const auto	cmp = a->lowerRelease.substr ( 0, 4 ).compare ( b->lowerRelease.substr ( 0, 4 ) );

				// Same release year, sort alphabetical
				if ( ! cmp )
					return lime::str::naturalCompare ( a->lowerName, b->lowerName ) < 0;

				return isForwards ? cmp < 0 : cmp > 0;
			} );
			break;

		case columnId::information:
			std::ranges::sort ( rowData, [ isForwards ] ( const auto a, const auto b ) {

				const auto	fA = a->flags & 0x30;
				const auto	fB = b->flags & 0x30;

				// Same chip-type, sort by year
				if ( fA == fB )
				{
					const auto	cmp = a->lowerRelease.substr ( 0, 4 ).compare ( b->lowerRelease.substr ( 0, 4 ) );

					// Same release year, sort alphabetical
					if ( ! cmp )
						return lime::str::naturalCompare ( a->lowerName, b->lowerName ) < 0;

					return isForwards ? cmp < 0 : cmp > 0;
				}

				return isForwards ? fA < fB : fB < fA;
			} );
			break;
	}
}
//-----------------------------------------------------------------------------

bool GUI_ListBox::keyPressed ( const juce::KeyPress& key )
{
	// Ctrl+A selects all rows, Ctrl+Shift+A deselects
	if ( key.getModifiers ().isCommandDown () && key.isKeyCode ( 'A' ) )
	{
		if ( key.getModifiers ().isShiftDown () )
			deselectAllRows ();
		else
			selectRangeOfRows ( 0, getNumRows () - 1, true );

		return true;
	}

	// Ctrl+Up/Down are global volume hot-keys: don't let the base class
	// swallow them as plain row navigation
	if ( key.getModifiers ().isCommandDown () && ( key.isKeyCode ( juce::KeyPress::upKey ) || key.isKeyCode ( juce::KeyPress::downKey ) ) )
		return false;

	return juce::TableListBox::keyPressed ( key );
}
//-----------------------------------------------------------------------------

void GUI_ListBox::componentVisibilityChanged ()
{
	if ( ! isShowing () )
		return;

	grabKeyboardFocus ();

	if ( getSelectedRows ().isEmpty () && getNumRows () > 0 )
		selectRow ( 0 );
}
//-----------------------------------------------------------------------------

void GUI_ListBox::resized ()
{
	juce::TableListBox::resized ();

	constexpr auto	minimumNameWidth = 270;

	auto&	header = getHeader ();

	// Get total width of all columns
	auto	newWidth = minimumNameWidth;

	const auto	numCols = header.getNumColumns ( false );
	for ( auto i = 0; i < numCols; ++i )
	{
		const auto	id = header.getColumnIdOfIndex ( i, false );
		if ( id == 0 || id == columnId::name )
			continue;

		newWidth += header.getColumnWidth ( id );
	}

	const auto	scrBarWidth = getViewport ()->getScrollBarThickness ();
	const auto	width = getWidth () - scrBarWidth * 2;

	// Not enough room for everything? Remove the Info column
	header.setColumnVisible ( columnId::information, width >= newWidth );
	if ( width < newWidth )
		newWidth -= header.getColumnWidth ( columnId::information );

	// Not enough room for everything? Remove the Release column
	header.setColumnVisible ( columnId::release, width >= newWidth );
	if ( width < newWidth )
		newWidth -= header.getColumnWidth ( columnId::release );

	// Resize name column to take up the remaining space
	header.setColumnWidth ( columnId::name, width - ( newWidth - minimumNameWidth ) );
}
//-----------------------------------------------------------------------------

int GUI_ListBox::getNumRows ()
{
	return int ( rowData.size () );
}
//-----------------------------------------------------------------------------

void GUI_ListBox::paintOverChildren ( juce::Graphics& g )
{
	juce::TableListBox::paintOverChildren ( g );

	if ( placeholderKey.isEmpty () || getNumRows () > 0 )
		return;

	g.setColour ( findColour ( UI::colors::textMuted ) );
	g.setFont ( UI::font ( UI::fonts::list_placeholder ) );
	g.drawFittedText ( strings->get ( placeholderKey ), getLocalBounds (), juce::Justification::centred, 3 );
}
//-----------------------------------------------------------------------------

void GUI_ListBox::paintRowBackground ( juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	if ( rowIsSelected )
		g.setColour ( UI::getShade ( UI::shades::selected ) );
	else if ( rowNumber == hoverPosition )
		g.setColour ( UI::getShade ( UI::shades::hover ) );
	else
		return;

	const auto	b = juce::Rectangle<int> ( width, height ).toFloat ();

	g.fillRoundedRectangle ( b, UI::corner ( UI::corners::browser_list_row, b ) );
}
//-----------------------------------------------------------------------------

void GUI_ListBox::paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	auto	b = juce::Rectangle<int> { width, height }.toFloat ().reduced ( 4.0f, 3.0f );

	const auto	col = findColour ( UI::colors::textMuted );
	const auto	txtCol = findColour ( UI::colors::text );
	g.setColour ( col );

	// A tune the database no longer resolves (deleted user tune, HVSC moved
	// it): the row stays visible in the error color, everything else inactive
	if ( ! rowData[ rowNumber ] )
	{
		g.setColour ( findColour ( UI::colors::statusError ) );
		g.setFont ( UI::font ( UI::fonts::browser_text ) );

		if ( columnId == columnId::number )
			g.drawText ( juce::String ( rowNumber + 1 ), b, juce::Justification::centred );
		else if ( columnId == columnId::name )
			g.drawText ( getMissingRowText ( rowNumber ), b, juce::Justification::centredLeft );

		return;
	}

	const auto& ent = *rowData[ rowNumber ];
	const auto	subTune = getRealSubtune ( rowNumber );

	switch ( columnId )
	{
		case columnId::number:
			{
				if ( rowPlaying == rowNumber )
				{
					GUI_AppLookAndFeel::drawPlaybackAnimation ( g, b.withSizeKeepingCentre ( 15.0f, 12.0f ), findColour ( UI::colors::accentBright ), animSpeed );
				}
				else
				{
					g.setFont ( UI::font ( UI::fonts::browser_text ) );
					g.drawText ( juce::String ( rowNumber + 1 ), b, juce::Justification::centred );
				}
			}
			break;

		case columnId::animation:
			{
				if ( rowPlaying == rowNumber )
					GUI_AppLookAndFeel::drawPlaybackAnimation ( g, b.withSizeKeepingCentre ( 15.0f, 12.0f ), findColour ( UI::colors::accentBright ), animSpeed );
			}
			break;

		case columnId::name:
			{
				// Draw thumbnail
				{
					constexpr auto	ratio = ( 320.0f * VIC2::truePalX ) / 200.0f;
					const auto	r = b.removeFromLeft ( b.getHeight () * ratio );

					if ( g.clipRegionIntersects ( r.getSmallestIntegerContainer () ) )
					{
						auto&	img = thumbnailCache->getThumbnail ( ent.file, ent.isNTSC (), [ safe = juce::Component::SafePointer<GUI_ListBox> ( this ), rowNumber, r ] {
							// The component may be gone by the time the render job finishes (deleted playlist view)
							if ( safe == nullptr )
								return;

							// Repaint only the thumbnail area of the row, not the entire row, to avoid unnecessary repaints of text and tags
							const auto	cp = safe->getCellPosition ( columnId::name, rowNumber, true );
							safe->repaint ( r.getSmallestIntegerContainer () + cp.getTopLeft () );
						} );

						const auto	gs = GUI_RoundedClip ( g, r, UI::corner ( UI::corners::browser_thumbnail, r ) );
						g.setOpacity ( 1.0f );
						img.draw ( g, r );
					}
				}
				b.removeFromLeft ( 6.0f );

				// Track name, author and tags
				b.reduce ( 0.0f, 4.5f );
				if ( g.clipRegionIntersects ( b.getSmallestIntegerContainer () ) )
				{
					// Track name
					{
						const auto&	font = UI::font ( UI::fonts::browser_text );
						g.setFont ( font );

						auto	area = b.removeFromTop ( b.getHeight () / 2.0f );

						g.setColour ( findColour ( rowPlaying == rowNumber ? UI::colors::accentBright : UI::colors::text ) );

						const auto	name = stringutils::extendedASCIItoUTF8 ( ent.name );
						if ( subTune != ent.startTune )
						{
							const auto	strWidth = juce::GlyphArrangement::getStringWidth ( font, name ) + 4.0f;
							g.drawText ( name, area.removeFromLeft ( strWidth ), juce::Justification::centredLeft );

							g.setFont ( UI::font ( UI::fonts::browser_small ) );
							g.setColour ( col );
							g.drawText ( "#" + juce::String ( subTune ), area, juce::Justification::centredLeft );
						}
						else
							g.drawText ( name, area, juce::Justification::centredLeft );
					}

					// Author
					{
						const auto	author = stringutils::extendedASCIItoUTF8 ( ent.author );

						const auto	smlFont = UI::font ( UI::fonts::browser_small );
						const auto	authWidth = juce::GlyphArrangement::getStringWidth ( smlFont, author );

						g.setFont ( smlFont );
						g.setColour ( rowIsSelected ? txtCol : col );
						g.drawText ( author, b.removeFromLeft ( authWidth + 4 ), juce::Justification::centredLeft );
					}

					// Tags
					{
						for ( const auto& tag : tags->getTagEntries () )
						{
							if ( ! tags->isTagged ( tag.name, ent.file ) )
								continue;

							const auto	r = b.removeFromLeft ( b.getHeight () );
							if ( r.getWidth () != b.getHeight () )
								break;

							g.setColour ( findColour ( tag.colorId ) );
							g.fillPath ( UI::getScaledPath ( icons->get ( tag.name ), r, 0, 0.15f ) );
						};
					}
				}

				// Filter, digi, one-shot & delayed start, in the sid_scanner's colors
				if ( buildinfo::isDeveloperMode () )
				{
					auto	r = juce::Rectangle<int> { width - 18, 0, 18, height }.toFloat ();

					r = r.withSizeKeepingCentre ( 2.0f * 8.0f + 2.0f, 2.0f * 8.0f + 2.0f );

					auto	leftCol = r.removeFromLeft ( 8.0f );
					r.removeFromLeft ( 2.0f );
					auto	rightCol = r;

					// An absent feature still shows its dot, just faded
					auto drawDot = [ &g ] ( const juce::Rectangle<float> dotRect, const bool on, const juce::Colour col )
					{
						g.setColour ( col.withMultipliedAlpha ( on ? 1.0f : 0.1f ) );
						g.fillEllipse ( dotRect );
					};

					drawDot ( leftCol.removeFromTop ( 8.0f ), ent.hasAnyFilter (), findColour ( UI::colors::filterOn ) );

					leftCol.removeFromTop ( 2.0f );
					drawDot ( leftCol.removeFromTop ( 8.0f ), ent.hasAnyDigi (), findColour ( UI::colors::digi ).withRotatedHue ( -0.2f ) );

					drawDot ( rightCol.removeFromTop ( 8.0f ), ent.hasAnyOneShot (), juce::Colours::red );

					rightCol.removeFromTop ( 2.0f );
					drawDot ( rightCol.removeFromTop ( 8.0f ), hvscDB->hasDelays ( ent.lowerFile ), juce::Colours::yellow );
				}
			}
			break;

		case columnId::release:
			{
				b.reduce ( 0.0f, 4.5f );

				auto	str = juce::String ( stringutils::extendedASCIItoUTF8 ( ent.release ) );

				g.setColour ( rowIsSelected ? txtCol : col );

				// Year
				{
					b.reduce ( 0.0f, 1.5f );
					g.setFont ( UI::font ( UI::fonts::browser_text ) );
					g.drawText ( str.upToFirstOccurrenceOf ( " ", false, false ), b.removeFromTop ( b.getHeight () / 2.0f ), juce::Justification::centredLeft );
				}

				// Party (Company or Author)
				{
					g.setFont ( UI::font ( UI::fonts::browser_small ) );
					g.drawText ( str.fromFirstOccurrenceOf ( " ", false, false ), b, juce::Justification::centredLeft );
				}
			}
			break;

		case columnId::information:
			{
				b.reduce ( 0.0f, 4.5f );

				// Chip type
				{
					g.setFont ( UI::font ( UI::fonts::browser_text ) );

					static const juce::String	typeStr[ 4 ] = { "Unknown", "6581", "8580", "Both" };

					auto	sid1Type = ( ent.flags >> 4 ) & 3;
					const auto	sid2Type = ( ent.flags >> 6 ) & 3;
					const auto	sid3Type = ( ent.flags >> 8 ) & 3;

					if ( ent.lowerFile.ends_with ( "_2sid.sid" ) || ent.lowerFile.ends_with ( "_3sid.sid" ) )
						sid1Type |= sid2Type | sid3Type;
					else if ( sid1Type == 3 )
						sid1Type = 1;

					g.drawText ( typeStr[ sid1Type ], b.removeFromTop ( b.getHeight () / 2.0f ), juce::Justification::centredLeft );
				}

				// Video standard
				{
					g.setFont ( UI::font ( UI::fonts::browser_small ) );

					const auto	videoStandard = ( ent.flags >> 2 ) & 3;
					static const juce::String	videoStr[ 4 ] = { "Unknown", "PAL", "NTSC", "Both" };

					g.drawText ( videoStr[ videoStandard ], b, juce::Justification::centredLeft );
				}
			}
			break;

		case columnId::length:
			{
				g.setFont ( UI::font ( UI::fonts::browser_text ) );

				const auto	lenMS = SID::getTuneLength ( ent.file, subTune );
				g.drawText ( SID::convertTimeToString ( lenMS ), b, juce::Justification::centredRight );
			}
			break;

		case columnId::liked:
			{
				auto		liked = likes->isLiked ( ent.file, subTune ? subTune : ent.startTune );
				const auto	isExact = liked;

				if ( ! liked && ! filterExactMatch )
					liked = likes->isLiked ( ent.file );

				auto	iconName = icons->get ( liked ? "liked" : "not-liked" );
				auto	padding = 0.2f;
				auto	offset = juce::Point<float> {};
				if ( liked && ! isExact )
				{
					iconName = icons->get ( "other-liked" );
					padding = 0.16f;
					offset = { 1.0f, 1.0f };
				}

				const auto&	p = UI::getScaledPath ( iconName, b.toFloat () + offset, 0, padding );

				g.setColour ( liked ? findColour ( UI::colors::tagLiked ) : col.withMultipliedAlpha ( 0.5f ) );
				g.fillPath ( p );
			}
			break;
	}
}
//-----------------------------------------------------------------------------

juce::String GUI_ListBox::getCellTooltip ( int rowNumber, int columnId )
{
	if ( columnId != columnId::liked || filterExactMatch )
		return {};

	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) || ! rowData[ rowNumber ] )
		return {};

	const auto&	ent = *rowData[ rowNumber ];
	const auto	subTune = getRealSubtune ( rowNumber );

	auto		liked = likes->isLiked ( ent.file, subTune );
	const auto	isExact = liked;

	if ( ! liked )
		liked = likes->isLiked ( ent.file );

	if ( liked && ! isExact )
		return strings->get ( "search/no_exact_like_tip" );

	return {};
}
//-----------------------------------------------------------------------------

void GUI_ListBox::cellClicked ( int rowNumber, int columnId, const juce::MouseEvent& e )
{
	juce::TableListBoxModel::cellClicked ( rowNumber, columnId, e );

	if ( columnId != columnId::liked || ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	const auto	ent = rowData[ rowNumber ];
	if ( ! ent )
		return;
	const auto	subTune = getRealSubtune ( rowNumber );

	likes->toggle ( ent->file, subTune );

	UI::repaintCell ( this, rowNumber, columnId );

	msg::LikeChanged { juce::String ( ent->file.data (), ent->file.size () ), subTune }.send ();
}
//-----------------------------------------------------------------------------

void GUI_ListBox::cellDoubleClicked ( int rowNumber, int columnId, const juce::MouseEvent& e )
{
	juce::TableListBoxModel::cellDoubleClicked ( rowNumber, columnId, e );

	if ( columnId == columnId::liked )
		return;

	juce::TableListBox::returnKeyPressed ( rowNumber );
}
//-----------------------------------------------------------------------------

void GUI_ListBox::addHeaderColumn ( int colId, bool sortable )
{
	struct columnProps
	{
		juce::String	name;
		int				width;
	};

	static const std::unordered_map<int, columnProps>	defaultCols =
	{
		{ columnId::number,			{ "#",			40 } },
		{ columnId::animation,		{ "",			40 } },
		{ columnId::name,			{ "Title",		250 } },
		{ columnId::release,		{ "Release",	170 } },
		{ columnId::information,	{ "Info",		70 } },
		{ columnId::length,			{ "Time",		70 } },
		{ columnId::historyDate,	{ "Date",		90 } },
		{ columnId::liked,			{ "",			30 } },
		{ columnId::exportProgress,	{ "Progress",	170 } },
	};

	const auto&	[ name, width ] = defaultCols.at ( colId );

	auto&	header = getHeader ();
	header.addColumn ( name, colId, width, width, colId == columnId::name ? -1 : width, juce::TableHeaderComponent::visible | ( sortable ? juce::TableHeaderComponent::sortable : 0 ) );

	auto&	props = header.getProperties ();

	// Add icons
	switch ( colId )
	{
		case columnId::number:
			props.set ( "colJust" + juce::String ( columnId::number ), juce::Justification::centred );
			break;

		case columnId::length:
			props.set ( "colJust" + juce::String ( columnId::length ), juce::Justification::centredRight );
			break;

		case columnId::historyDate:
			props.set ( "colOff" + juce::String ( columnId::historyDate ), 45.0f );
			props.set ( "colOffY" + juce::String ( columnId::historyDate ), 8.0f );
			props.set ( "colIcon" + juce::String ( columnId::historyDate ), icons->get ( "list/history_date" ) );
			break;

		case columnId::exportProgress:
			props.set ( "colOff" + juce::String ( columnId::exportProgress ), 14.0f );
			break;
	}
}
//-----------------------------------------------------------------------------

void GUI_ListBox::timerUpdate ( const float secondsPassed )
{
	if ( ! isShowing () )
		return;

	if ( rowPlaying >= getNumRows () )
		rowPlaying = -1;

	if ( useNameOnly )
	{
		if ( tunePlaying.empty () )
		{
			rowPlaying = -1;
			return;
		}

		// Find row that is currently playing by name
		if ( rowPlaying < 0 || ! rowData[ rowPlaying ] || rowData[ rowPlaying ]->lowerFile != tunePlaying )
		{
			rowPlaying = -1;

			auto&	vp = *getViewport ();

			const auto	rowH = getRowHeight ();
			const auto	numNeeded = 4 + vp.getMaximumVisibleHeight () / rowH;
			const auto	y = vp.getViewPositionY ();

			const auto	firstIndex = y / rowH;
			const auto	lastIndex = std::min ( getNumRows () - 1, firstIndex + numNeeded );

			for ( auto i = firstIndex; i <= lastIndex; ++i )
			{
				if ( rowData[ i ] && rowData[ i ]->lowerFile == tunePlaying )
				{
					rowPlaying = i;
					break;
				}
			}
		}
	}

	if ( rowPlaying < 0 )
		return;

	animSpeed += secondsPassed * 2.0f;

	UI::repaintCell ( this, rowPlaying, columnId::number );
	UI::repaintCell ( this, rowPlaying, columnId::animation );
}
//-------------------------------------------------------------------------------------------------

void GUI_ListBox::setPlayingName ( const std::string& tuneName )
{
	const auto	oldRow = rowPlaying;

	tunePlaying = tuneName;
	rowPlaying = -1;
	useNameOnly = true;

	if ( oldRow >= 0 && oldRow < getNumRows () )
		repaintRow ( oldRow );
}
//-------------------------------------------------------------------------------------------------

void GUI_ListBox::setPlayingRow ( const int rowNumber )
{
	const auto	oldRow = rowPlaying;

	tunePlaying = "";
	rowPlaying = rowNumber >= int ( rowData.size () ) ? -1 : rowNumber;
	useNameOnly = false;

	if ( oldRow >= 0 && oldRow < getNumRows () )
		repaintRow ( oldRow );
}
//-------------------------------------------------------------------------------------------------

void GUI_ListBox::showRow ( const int rowNumber )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	selectRow ( rowNumber );
}
//-------------------------------------------------------------------------------------------------

int GUI_ListBox::findRow ( const std::string& lowerFile, const int subtune ) const
{
	auto	fileMatch = -1;

	for ( auto i = 0; i < int ( rowData.size () ); ++i )
	{
		if ( ! rowData[ i ] || rowData[ i ]->lowerFile != lowerFile )
			continue;

		if ( getRealSubtune ( i ) == subtune )
			return i;

		if ( fileMatch < 0 )
			fileMatch = i;
	}

	return fileMatch;
}
//-------------------------------------------------------------------------------------------------

const Database::entry* GUI_ListBox::getRow ( const int rowNumber )
{
	if ( rowNumber >= int ( rowData.size () ) )
		return nullptr;

	return rowData[ rowNumber ];
}
//-------------------------------------------------------------------------------------------------

void GUI_ListBox::changeListenerCallback ( juce::ChangeBroadcaster* source )
{
	if ( source == &hover )
	{
		const auto	oldHover = hoverPosition;
		hoverPosition = hover.getHoverPos ();
		if ( oldHover >= 0 )
			repaintRow ( oldHover );

		repaintRow ( hoverPosition );
	}
}
//-----------------------------------------------------------------------------

int GUI_ListBox::getRealSubtune ( const int rowNumber ) const
{
	auto	ent = rowData[ rowNumber ];
	if ( ! ent )
		return rowSubtune.empty () ? 0 : rowSubtune[ rowNumber ];

	if ( rowSubtune.empty () )
		return ent->startTune;

	if ( auto subTune = rowSubtune[ rowNumber ]; subTune != 0 )
		return subTune;

	return ent->startTune;
}
//-----------------------------------------------------------------------------

juce::StringArray GUI_ListBox::getTuneList ( const juce::SparseSet<int>& rows, const bool withSubtunes ) const
{
	// There are no subtunes in the list, but the caller requested them
	jassert ( ! ( withSubtunes && rowSubtune.empty () ) );

	juce::StringArray	selectedTunes;

	for ( auto i = 0; i < rows.size (); ++i )
	{
		if ( ! rowData[ rows[ i ] ] )	// Missing tunes have no file to act on
			continue;

		const auto&	file = rowData[ rows[ i ] ]->file;
		const auto	name = juce::String ( file.data (), file.size () );

		// The playlist dialect leaves the default tune bare, no ",0"
		if ( const auto subtune = withSubtunes ? rowSubtune[ rows[ i ] ] : int16_t ( 0 ); subtune != 0 )
			selectedTunes.add ( name + "," + juce::String ( subtune ) );
		else if ( withSubtunes )
			selectedTunes.add ( name );
		else
			selectedTunes.addIfNotAlreadyThere ( name );
	}

	return selectedTunes;
}
//-----------------------------------------------------------------------------

juce::var GUI_ListBox::getDragSourceDescription ( const juce::SparseSet<int>& rowsToDescribe )
{
	// The payload speaks the search-drag dialect, so playlists and the playlist
	// grid accept rows from any list; rows whose tune is gone are skipped
	juce::Array<juce::var>	tunes;

	for ( auto i = 0; i < rowsToDescribe.size (); ++i )
	{
		const auto	row = rowsToDescribe[ i ];

		if ( ! rowData[ row ] )
			continue;

		const auto&	file = rowData[ row ]->file;

		tunes.add ( juce::String ( file.data (), file.size () ) + "," + juce::String ( rowSubtune.empty () ? 0 : rowSubtune[ row ] ) );
	}

	if ( tunes.isEmpty () )
		return {};

	auto*	desc = new juce::DynamicObject ();
	desc->setProperty ( "source", "search" );
	desc->setProperty ( "tunes", tunes );

	return desc;
}
//-----------------------------------------------------------------------------

juce::String GUI_ListBox::getTuneFolder ( const juce::SparseSet<int>& rows ) const
{
	juce::StringArray	selectedTunes;

	for ( auto i = 0; i < rows.size (); ++i )
		if ( rowData[ rows[ i ] ] )
			selectedTunes.addIfNotAlreadyThere ( juce::String ( rowData[ rows[ i ] ]->file.data (), rowData[ rows[ i ] ]->file.size () ).upToLastOccurrenceOf ( "/", true, false ) );

	if ( selectedTunes.size () != 1 )
		return {};

	// User tunes all live in the same Tunes folder, there is no folder to go
	// to; empty disables the menu entry
	if ( selectedTunes[ 0 ].startsWith ( "$USER$" ) )
		return {};

	return selectedTunes[ 0 ].fromFirstOccurrenceOf ( "/", true, false );
}
//-----------------------------------------------------------------------------
