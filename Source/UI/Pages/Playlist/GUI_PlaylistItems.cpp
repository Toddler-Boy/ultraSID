#include <JuceHeader.h>

#include "GUI_PlaylistItems.h"

#include "ultra-shared/Config/BuildInfo.h"
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
	addHeaderColumn ( columnId::name );
	addHeaderColumn ( columnId::release );
	addHeaderColumn ( columnId::information );
	addHeaderColumn ( columnId::length );
	addHeaderColumn ( columnId::liked );

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

	if ( dragOverRow < 0 )
		return;

	const auto	insertRect = getRowPosition ( dragOverRow, true ).toFloat ().withHeight ( 3.0 ).reduced ( 4.0, 0.0f );

	g.setColour ( juce::Colours::lime.withAlpha ( 0.5f ) );
	g.fillRoundedRectangle ( insertRect, insertRect.getHeight () / 2.0f );
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
	const auto	source = dragSourceDetails.description.getProperty ( "source", {} ).toString ();

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

	dragOverRow = -1;
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::itemDragEnter ( const SourceDetails& dragSourceDetails )
{
	beginDragAutoRepeat ( 50 );

	dragOverRow = getInsertionIndexForPosition ( dragSourceDetails.localPosition.getX (), dragSourceDetails.localPosition.getY () );
	repaint ();
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::itemDragMove ( const SourceDetails& dragSourceDetails )
{
	getViewport ()->autoScroll ( dragSourceDetails.localPosition.getX (), dragSourceDetails.localPosition.getY () - getHeader ().getHeight (), 30, 120 );

	dragOverRow = getInsertionIndexForPosition ( dragSourceDetails.localPosition.getX (), dragSourceDetails.localPosition.getY () );
	repaint ();
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::itemDragExit ( const SourceDetails& /*dragSourceDetails*/ )
{
	dragOverRow = -1;
	repaint ();
}
//-------------------------------------------------------------------------------------------------

void GUI_PlaylistItems::updateRowData ()
{
	realPlaylist->createRowData ( rowData, rowSubtune );
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
