#include <JuceHeader.h>

#include "GUI_HistoryItems.h"

#include "ultra-shared/Config/BuildInfo.h"
#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "Config/FilePaths.h"
#include "Database/Database.h"
#include "UI/ui-colors.h"
#include "UI/UI_Menus.h"

#include "../GUI_Pages.h"

//-----------------------------------------------------------------------------

struct Interval
{
	Interval ( const juce::Time& compDate, const juce::Time& nowDate )
	{
		auto diff = nowDate - compDate;
		if ( diff.inSeconds () < 0 )
		{
			invert = true;
			diff = juce::RelativeTime ( -diff.inSeconds () );
		}

		y = (int)std::floor ( diff.inDays () / 365.0 );
		diff -= juce::RelativeTime::days ( y * 365.0 );

		m = (int)std::floor ( diff.inDays () / 30.0 );
		diff -= juce::RelativeTime::days ( m * 30.0 );

		d = (int)std::floor ( diff.inDays () );
		diff -= juce::RelativeTime::days ( d );

		h = (int)std::floor ( diff.inHours () );
		diff -= juce::RelativeTime::hours ( h );

		i = (int)std::floor ( diff.inMinutes () );
		diff -= juce::RelativeTime::minutes ( i );

		s = (int)std::floor ( diff.inSeconds () );
		diff -= juce::RelativeTime::days ( s );
	}

	bool invert = false;
	int y = 0;
	int m = 0;
	int d = 0;
	int h = 0;
	int i = 0;
	int s = 0;
};
//-------------------------------------------------------------------------------------------------

static juce::String getDiffString ( int value, juce::String name, int length = 0 )
{
	if ( ! value )
		return "";

	if ( length )
		return juce::String ( value ) + name.substring ( 0, length );

	auto outStr = " " + juce::String ( value ) + " " + name;
	if ( value != 1 )
		outStr += "s";

	return outStr;
}
//-------------------------------------------------------------------------------------------------

static juce::String smartTime ( juce::Time compDate, juce::Time nowDate, bool shrt )
{
	if ( compDate == juce::Time () )
		return "never";

	auto interval = Interval ( nowDate, compDate );

	juce::String outStr = "";

	// Less than one days
	if ( interval.y == 0 && interval.m == 0 && interval.d <= 1 )
	{
		// Show as hours only
		interval.h += interval.d * 24;
		interval.d = 0;
	}
	else
	{
		outStr += getDiffString ( interval.y, "year", shrt ? 1 : 0 );
		outStr += getDiffString ( interval.m, "month", shrt ? 1 : 0 );

		// Less than a year, show the days
		if ( interval.y == 0 )
			outStr += getDiffString ( interval.d, "day", shrt ? 1 : 0 );
	}

	if ( outStr.isEmpty () )
	{
		outStr += getDiffString ( interval.h, "hour", shrt ? 1 : 0 );
		outStr += getDiffString ( interval.i, "minute", shrt ? 3 : 0 );

		// Less than a minute, show seconds
		if ( outStr.isEmpty () )
			outStr += getDiffString ( interval.s, "second", shrt ? 1 : 0 );
	}

	if ( interval.invert )
	{
		// Less than 10 seconds, show "just now"
		if ( interval.y == 0 && interval.m == 0 && interval.d == 0 && interval.h == 0 && interval.i == 0 && interval.s < 10 )
			outStr = "just now";
	}

	outStr = outStr.trim ();

	return outStr;
}
//-------------------------------------------------------------------------------------------------

GUI_HistoryItems::GUI_HistoryItems ( GUI_Pages& _pages )
	: pages ( _pages )
{
	setName ( "results" );

	addHeaderColumn ( columnId::animation );
	addHeaderColumn ( columnId::name );
	addHeaderColumn ( columnId::release );
	addHeaderColumn ( columnId::information );
	addHeaderColumn ( columnId::length );
	addHeaderColumn ( columnId::historyDate );
	addHeaderColumn ( columnId::liked );

	placeholderKey = "history/empty";
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::reload ()
{
	const auto&	entries = history->getEntries ();

	rowData.clear ();
	rowSubtune.clear ();

	// History records what was deliberately loaded, so a row outlives its tune
	for ( const auto& entry : entries )
	{
		rowData.push_back ( db::findDatabaseEntry ( entry.file ) );
		rowSubtune.push_back ( static_cast<int16_t> ( entry.subtune ) );
	}

	updateContent ();

	// Rows may vanish without new ones taking their place, and the placeholder
	// spans the full component: the partial row repaint doesn't cover it
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::clearAll ()
{
	deselectAllRows ();
	history->clearAll ();
	reload ();
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::clearOlderThan ( const double days )
{
	deselectAllRows ();
	history->clearOlderThan ( days );
	reload ();
}
//-----------------------------------------------------------------------------

juce::String GUI_HistoryItems::getMissingRowText ( const int rowNumber ) const
{
	return filepaths::stripLocationMarker ( history->getEntries ()[ rowNumber ].file );
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::refreshRowData ()
{
	reload ();
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	auto	b = juce::Rectangle<int> { width, height }.toFloat ().reduced ( 4.0f, 8.0f ).withTrimmedLeft ( 10.0f );

	switch ( columnId )
	{
		case columnId::historyDate:
		{
			g.setColour ( findColour ( UI::colors::textMuted ) );
			g.setFont ( UI::font ( UI::fonts::browser_text ) );
			g.drawText ( smartTime ( history->getEntries ()[ rowNumber ].time, juce::Time::getCurrentTime (), true ), b, juce::Justification::centred );
		}
		break;

		default:
			GUI_ListBox::paintCell ( g, rowNumber, columnId, width, height, rowIsSelected );
	}
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::returnKeyPressed ( int lastRowSelected )
{
	// A row whose tune is gone stays listed, but there is nothing to play
	const auto	ent = rowData[ lastRowSelected ];
	if ( ! ent )
		return;

	pages.setCurrentPlaylist ( nullptr );
	pages.loadTune ( juce::String ( ent->file.data (), ent->file.size () ), rowSubtune[ lastRowSelected ], "history", - 1 );
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::cellClicked ( int row, int columnId, const juce::MouseEvent& e )
{
	GUI_ListBox::cellClicked ( row, columnId, e );

	if ( ! e.mods.isPopupMenu () )
		return;

	auto	m = UI::newPopupMenu ( *this );

	const auto	rows = getSelectedRows ();
	const auto	selectedTunes = getTuneList ( rows );
	const auto	tuneFolder = getTuneFolder ( rows );

	// Add to playlist
	UI::menu_AddToPlaylist ( m, selectedTunes );

	// Go to artist
	m.addSeparator ();
	UI::menu_GoToFolder ( m, tuneFolder );

	// Export track
	m.addSeparator ();
	UI::menu_ExportTrack ( m, selectedTunes );

	m.addSeparator ();

	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	m.addItem ( UI::newDangerousMenuItem ( strings->get ( "menu/remove_from_history" ), icons->get ( "menu/delete" ), [ this, rows ]
	{
		std::vector<int>	indices;
		for ( auto i = 0; i < rows.size (); ++i )
			indices.push_back ( rows[ i ] );

		deselectAllRows ();
		history->remove ( indices );
		reload ();
	} ) );

	if ( buildinfo::isDeveloperMode () )
	{
		m.addSeparator ();
		UI::menu_ToggleTag ( m, getTuneList ( rows, false ) );
	}

	UI::showMenuAtMouse ( m, *this );
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::visibilityChanged ()
{
	GUI_ListBox::visibilityChanged ();

	if ( isShowing () )
		timerCallback ();
	else
		stopTimer ();
}
//-----------------------------------------------------------------------------

void GUI_HistoryItems::timerCallback ()
{
	const auto&	entries = history->getEntries ();

	if ( entries.empty () )
	{
		stopTimer ();
		return;
	}

	// Every second while the newest play is under a minute old, else every 15 seconds
	const auto	interval = Interval ( entries[ 0 ].time, juce::Time::getCurrentTime () );
	const auto	minuteUpdates = interval.y || interval.m || interval.d || interval.h || interval.i;
	startTimer ( minuteUpdates ? 15'000 : 1'000 );

	// Repaint the date cells
	UI::repaintColumn ( this, columnId::historyDate );
}
//-----------------------------------------------------------------------------
