#include <JuceHeader.h>

#include "GUI_PlaylistItems.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Config/FilePaths.h"
#include "Database/Database.h"
#include "Database/TuneInfo.h"
#include "Helpers/Messages.h"
#include "UI/UI_Menus.h"

#include "../GUI_Pages.h"

namespace
{
	juce::String sparseSetToString ( const juce::SparseSet<int>& set )
	{
		juce::StringArray	rangeStrings;

		for ( auto i = 0; i < set.getNumRanges (); ++i )
		{
			const auto	range = set.getRange ( i );

			rangeStrings.add ( juce::String ( range.getStart () ) + "-" + juce::String ( range.getEnd () ) );
		}

		return rangeStrings.joinIntoString ( "," );
	}

	juce::SparseSet<int> stringToSparseSet ( const juce::String& s )
	{
		juce::SparseSet<int>	set;

		for ( auto& rs : juce::StringArray::fromTokens ( s, ",", "" ) )
		{
			auto start = rs.upToFirstOccurrenceOf ( "-", false, false ).getIntValue ();
			auto end = rs.fromFirstOccurrenceOf ( "-", false, false ).getIntValue ();

			if ( end > start )
				set.addRange ( juce::Range<int> ( start, end ) );
		}

		return set;
	}
}
//-----------------------------------------------------------------------------

GUI_PlaylistItems::GUI_PlaylistItems ( GUI_Pages& _browser, const juce::String& name )
	: browser ( _browser )
{
	setName ( name );

	addHeaderColumn ( columnId::number );
	addHeaderColumn ( columnId::name, true );
	addHeaderColumn ( columnId::release, true );
	addHeaderColumn ( columnId::information, true );
	addHeaderColumn ( columnId::length, true );
	addHeaderColumn ( columnId::liked );

	syncHeaderSort ();

	placeholderKey = "playlist/empty";
}
//-----------------------------------------------------------------------------

void GUI_PlaylistItems::setName ( const juce::String& name )
{
	GUI_ListBox::setName ( name );

	realPlaylist = playlists->getPlaylistItems ( name.toStdString () );

	// You have to create the real playlist before creating the view
	jassert ( realPlaylist );

	if ( ! realPlaylist )
		return;

	realPlaylist->setRowPlayingLocation ( &rowPlaying );
	realPlaylist->createRowData ( rowData, rowSubtune );

	// The constructor syncs itself once the columns exist
	if ( getHeader ().getNumColumns ( false ) > 0 )
		syncHeaderSort ();
}
//-----------------------------------------------------------------------------

void GUI_PlaylistItems::syncHeaderSort ()
{
	if ( realPlaylist )
		getHeader ().setSortColumnId ( columnForSortKey ( realPlaylist->getSortKey () ), realPlaylist->isSortedForwards () );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistItems::sortOrderChanged ( int newSortColumnId, bool isForwards )
{
	if ( ! realPlaylist )
		return;

	// Already the playlist's state: a header sync
	const auto	key = sortKeyForColumn ( newSortColumnId );
	if ( key == realPlaylist->getSortKey () && isForwards == realPlaylist->isSortedForwards () )
		return;

	const auto	selected = getSelectedRows ();

	std::vector<int>	selectedEntries;
	selectedEntries.reserve ( size_t ( selected.size () ) );

	for ( auto i = 0; i < selected.size (); ++i )
		selectedEntries.push_back ( realPlaylist->getEntryIndex ( selected[ i ] ) );

	realPlaylist->setSort ( key, isForwards );
	updateRowData ();

	juce::SparseSet<int>	rows;
	for ( const auto entry : selectedEntries )
	{
		const auto	row = realPlaylist->getViewIndex ( entry );
		rows.addRange ( { row, row + 1 } );
	}

	setSelectedRows ( rows, juce::dontSendNotification );

	if ( ! rows.isEmpty () )
		scrollToEnsureRowIsOnscreen ( rows[ 0 ] );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistItems::returnKeyPressed ( int lastRowSelected )
{
	// Missing tunes can't be played
	if ( ! rowData[ lastRowSelected ] )
		return;

	browser.setCurrentPlaylist ( this );
	const auto&	file = rowData[ lastRowSelected ]->file;
	browser.loadTune ( juce::String ( file.data (), file.size () ), rowSubtune[ lastRowSelected ], "playlist", lastRowSelected);
}
//-----------------------------------------------------------------------------

juce::String GUI_PlaylistItems::getMissingRowText ( const int rowNumber ) const
{
	// The raw playlist entry, without the internal $HVSC$/$USER$ marker
	const auto [ tuneName, subTune ] = SID::parseTuneName ( realPlaylist->getEntry ( rowNumber ) );

	return filepaths::stripLocationMarker ( tuneName );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistItems::cellClicked ( int row, int columnId, const juce::MouseEvent& e )
{
	GUI_ListBox::cellClicked ( row, columnId, e );

	if ( ! e.mods.isPopupMenu () )
		return;

	auto	m = UI::newPopupMenu ( *this );

	const auto	rows = getSelectedRows ();
	const auto	selectedTunes = getTuneList ( rows );

	UI::menu_AddToPlaylist ( m, selectedTunes );
	UI::menu_RemoveFromPlaylist ( m, getName (), rows );

	m.addSeparator ();

	UI::menu_GoToFolder ( m, getTuneFolder ( rows ) );

	m.addSeparator ();

	UI::menu_ExportTrack ( m, selectedTunes );

	m.addSeparator ();

	UI::menu_MoveItems ( m, getName (), rows );

	if ( buildinfo::isDeveloperMode () )
	{
		m.addSeparator ();
		UI::menu_ToggleTag ( m, getTuneList ( rows, false ) );
	}

	UI::showMenuAtMouse ( m, *this );
}
//-----------------------------------------------------------------------------

juce::var GUI_PlaylistItems::getDragSourceDescription ( const juce::SparseSet<int>& rowsToDescribe )
{
	// Row ranges are digits, '-' and ',' only, so they carry no user content
	auto*	desc = new juce::DynamicObject ();
	desc->setProperty ( "source", "playlist" );
	desc->setProperty ( "playlist", getName () );
	desc->setProperty ( "rows", sparseSetToString ( rowsToDescribe ) );

	return desc;
}
//-----------------------------------------------------------------------------

void GUI_PlaylistItems::paintOverChildren ( juce::Graphics& g )
{
	GUI_ListBox::paintOverChildren ( g );

	if ( ! dragIsOver )
		return;

	g.setColour ( findColour ( UI::colors::statusOk ) );

	// The list's two ends get an arrow pointing at the line, from inside the list
	auto drawLineWithArrow = [ &g ] ( const juce::Rectangle<float>& line, const bool pointsDown )
	{
		constexpr auto	arrowSize = 17.0f;
		constexpr auto	gap = 5.0f;

		const juce::SharedResourcePointer<Icons>	icons;

		const auto	arrowRect = juce::Rectangle<float> ( arrowSize, arrowSize )
									.withCentre ( { line.getCentreX (), pointsDown ? line.getY () - gap - arrowSize / 2.0f : line.getBottom () + gap + arrowSize / 2.0f } );

		g.fillRoundedRectangle ( line, line.getHeight () / 2.0f );
		g.fillPath ( UI::getScaledPath ( icons->get ( pointsDown ? "menu/move_to_bottom" : "menu/move_to_top" ), arrowRect ) );
	};

	if ( dragOverRow >= 0 )
	{
		const auto	insertRect = getRowPosition ( dragOverRow, true ).toFloat ().withHeight ( 3.0 ).reduced ( 4.0, 0.0f );

		// The new top position
		if ( dragOverRow == 0 )
			return drawLineWithArrow ( insertRect, false );

		// Below the last row of a full list the bar lands outside and gets clipped
		if ( dragOverRow < getNumRows () || insertRect.getBottom () <= float ( getHeight () ) )
		{
			g.fillRoundedRectangle ( insertRect, insertRect.getHeight () / 2.0f );
			return;
		}
	}

	// Drops that append: a line along the bottom
	drawLineWithArrow ( getLocalBounds ().toFloat ().removeFromBottom ( 7.0f ).withHeight ( 3.0f ).reduced ( 4.0f, 0.0f ), true );
}
//-----------------------------------------------------------------------------

void GUI_PlaylistItems::paintRowBackground ( juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	juce::Colour	col;
	if ( rowIsSelected )
		col = UI::getShade ( UI::shades::selected );
	else if ( rowNumber == hoverPosition )
		col = UI::getShade ( UI::shades::hover );
	else
		return;

	col = col.withBrightness ( 1.0f ).withMultipliedAlpha ( col.getBrightness () * 0.66f );
	g.setColour ( col );

	const auto	b = juce::Rectangle<int> ( width, height ).toFloat ();

	g.fillRoundedRectangle ( b, UI::corner ( UI::corners::browser_list_row, b ) );
}
//-----------------------------------------------------------------------------

bool GUI_PlaylistItems::isInterestedInDragSource ( const SourceDetails& dragSourceDetails )
{
	const auto&	desc = dragSourceDetails.description;
	const auto	source = desc.getProperty ( "source", {} ).toString ();

	// No reordering while sorted
	if ( source == "playlist" && realPlaylist->isSorted () && desc.getProperty ( "playlist", {} ).toString () == getName () )
		return false;

	return source == "playlist" || source == "STIL" || source == "search";
}
//----------------------------------------------------------------------------------

void GUI_PlaylistItems::itemDropped ( const SourceDetails& dragSourceDetails )
{
	// A drop on a playlist card has no row under the mouse, and appending must stay
	// appending for every tune in the drag
	auto	insertPos = dragOverRow;

	auto	addInOrder = [ this, &insertPos ] ( const std::string& tune )
	{
		realPlaylist->addItem ( tune, insertPos );

		if ( insertPos >= 0 )
			++insertPos;
	};

	const auto&	desc = dragSourceDetails.description;
	const auto	source = desc.getProperty ( "source", {} ).toString ();

	if ( source == "playlist" )
	{
		// Move items between playlists or within playlist
		const auto	sourceList = desc.getProperty ( "playlist", {} ).toString ();
		const auto	rows = stringToSparseSet ( desc.getProperty ( "rows", {} ).toString () );

		if ( sourceList == getName () )
		{
			// Move within playlist
			realPlaylist->moveItems ( rows, insertPos );
		}
		else
		{
			// Add from another playlist
			auto	sourcePlaylist = playlists->getPlaylistItems ( sourceList.toStdString () );
			for ( auto i = 0; i < rows.size (); ++i )
				realPlaylist->addItem ( sourcePlaylist->getEntry ( rows[ i ] ) );
		}
	}
	else if ( source == "STIL" )
	{
		// Drag from STIL list
		const auto	tune = desc.getProperty ( "tune", {} ).toString ();

		if ( const auto* subtunes = desc.getProperty ( "subtunes", {} ).getArray () )
			for ( const auto& subtune : *subtunes )
				addInOrder ( ( tune + "," + subtune.toString () ).toStdString () );
	}
	else if ( source == "search" )
	{
		// Drag from search result
		if ( const auto* tunes = desc.getProperty ( "tunes", {} ).getArray () )
			for ( const auto& tune : *tunes )
				addInOrder ( tune.toString ().toStdString () );
	}

	realPlaylist->save ();
	realPlaylist->createShuffle ();
	updateRowData ();

	msg::PlaylistUpdate { getName () }.send ();

	if ( dragOverRow >= 0 )
		selectRow ( std::clamp ( dragOverRow, 0, int ( rowData.size () ) - 1 ) );

	dragIsOver = false;
	dragOverRow = -1;
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::itemDragEnter ( const SourceDetails& dragSourceDetails )
{
	beginDragAutoRepeat ( 50 );

	dragIsOver = true;
	dragOverRow = getDropRow ( dragSourceDetails );
	repaint ();
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::itemDragMove ( const SourceDetails& dragSourceDetails )
{
	getViewport ()->autoScroll ( dragSourceDetails.localPosition.getX (), dragSourceDetails.localPosition.getY () - getHeader ().getHeight (), 30, 120 );

	dragOverRow = getDropRow ( dragSourceDetails );
	repaint ();
}
//-------------------------------------------------------------------------------------------------

int GUI_PlaylistItems::getDropRow ( const SourceDetails& dragSourceDetails ) const
{
	// Sorted, drops append
	if ( realPlaylist->isSorted () )
		return -1;

	return getInsertionIndexForPosition ( dragSourceDetails.localPosition.getX (), dragSourceDetails.localPosition.getY () );
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::itemDragExit ( const SourceDetails& /*dragSourceDetails*/ )
{
	dragIsOver = false;
	dragOverRow = -1;
	repaint ();
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::updateRowData ()
{
	realPlaylist->createRowData ( rowData, rowSubtune );
	syncHeaderSort ();
	updateContent ();
	repaint ();
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::clear ()
{
	realPlaylist->clear ();
	updateRowData ();
}
//-----------------------------------------------------------------------------
