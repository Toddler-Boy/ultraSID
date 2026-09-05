#include <JuceHeader.h>

#include "GUI_ExportItems.h"

#include "libSidplayEZ/src/EZ/tinyCSV.h"

#include "ultra-shared/Helpers/FileUtils.h"
#include "ultra-shared/Resources/Icons.h"
#include "ultra-shared/Resources/Strings.h"
#include "ultra-shared/UI/Components/GUI_RoundedClip.h"
#include "ultra-shared/UI/UI_Helpers.h"

#include "App/ExportName.h"
#include "Config/FilePaths.h"
#include "Config/Preferences.h"
#include "Database/Database.h"
#include "Database/TuneInfo.h"
#include "Helpers/Messages.h"
#include "UI/ui-colors.h"

#include "GUI_Export.h"

//-----------------------------------------------------------------------------

// The saved list's TinyCSV header. Only what can't be reconstructed is
// stored: the tune's full path is rebuilt from its key, render parameters
// come fresh from the database, and only a completed export keeps its
// exportFilename, the absolute path of the file it actually wrote. The
// path columns are written quoted, they can contain commas
constexpr auto	csvHeader = "tune,status,date,exportFilename\n";

//-----------------------------------------------------------------------------

GUI_ExportItems::GUI_ExportItems ()
{
	setName ( "exportItems" );

	addHeaderColumn ( columnId::number );
	addHeaderColumn ( columnId::name );
	addHeaderColumn ( columnId::release );
	addHeaderColumn ( columnId::information );
	addHeaderColumn ( columnId::length );
	addHeaderColumn ( columnId::exportProgress );

	placeholderKey = "export/empty";
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::repaintProgressCell ( const int index )
{
	// Bounds-checked at delivery: rows may have been erased since the worker
	// sent the message
	const auto	row = toQueueIndex ( index );
	if ( ! juce::isPositiveAndBelow ( row, getNumRows () ) )
		return;

	UI::repaintCell ( this, row, columnId::exportProgress );

	// Terminal states are what the saved list is made of
	const auto	status = tuneExporter->getStatus ( index );
	if ( status == TuneExporter::COMPLETE || status == TuneExporter::CANCELED || status == TuneExporter::ERROR )
		save ();
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::returnKeyPressed ( int lastRowSelected )
{
	// A row whose tune is gone stays listed, but there is nothing to play
	const auto	ent = rowData[ lastRowSelected ];
	if ( ! ent )
		return;

	msg::LoadTune { juce::String ( ent->file.data (), ent->file.size () ), rowSubtune[ lastRowSelected ], "export", -1 }.send ();
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::cellClicked ( int row, int columnId, const juce::MouseEvent& e )
{
	GUI_ListBox::cellClicked ( row, columnId, e );

	if ( ! e.mods.isPopupMenu () )
		return;

	auto	m = UI::newPopupMenu ( *this );

	const juce::SharedResourcePointer<Strings>	strings;
	const juce::SharedResourcePointer<Icons>	icons;

	const auto	rows = getSelectedRows ();

	// Reveal a completed export's file, platform-correctly worded
	{
#if JUCE_WINDOWS
		constexpr auto	showKey = "menu/show_in_explorer";
#elif JUCE_MAC
		constexpr auto	showKey = "menu/show_in_finder";
#else
		constexpr auto	showKey = "menu/show_in_file_manager";
#endif

		auto	file = juce::File ();

		if ( rows.size () == 1 && tuneExporter->getStatus ( toQueueIndex ( rows[ 0 ] ) ) == TuneExporter::COMPLETE )
			file = juce::File ( tuneExporter->getEntry ( toQueueIndex ( rows[ 0 ] ) ).exportFilename );

		m.addItem ( UI::newMenuItem ( strings->get ( showKey ), icons->get ( "menu/go_to_folder" ), [ file ]
		{
			// The file may vanish while the menu is open
			if ( file.existsAsFile () )
				file.revealToUser ();
		} ).setEnabled ( file.existsAsFile () ) );
	}

	m.addSeparator ();

	auto anySelected = [ &rows ] ( const auto& eligible )
	{
		for ( auto i = 0; i < rows.size (); ++i )
			if ( eligible ( rows[ i ] ) )
				return true;

		return false;
	};

	// Canceled with the tune still around = re-exportable
	auto canReAdd = [ this ] ( const int row )
	{
		return rowData[ row ] != nullptr && tuneExporter->getStatus ( toQueueIndex ( row ) ) == TuneExporter::CANCELED;
	};

	// Live work (queued, rendering, paused) = cancelable
	auto canCancel = [ this ] ( const int row )
	{
		const auto	status = tuneExporter->getStatus ( toQueueIndex ( row ) );

		return status != TuneExporter::COMPLETE && status != TuneExporter::CANCELED && status != TuneExporter::ERROR;
	};

	// Re-add to export queue
	m.addItem ( UI::newMenuItem ( strings->get ( rows.size () > 1 ? "menu/export_tunes" : "menu/export_tune" ), icons->get ( "menu/requeue_export" ), [ this, rows ]
	{
		for ( auto i = 0; i < rows.size (); ++i )
			reAddRow ( rows[ i ] );

	} ).setEnabled ( anySelected ( canReAdd ) ) );

	m.addSeparator ();

	// Remove from export queue
	m.addItem ( UI::newDangerousMenuItem ( strings->get ( rows.size () > 1 ? "menu/cancel_exports" : "menu/cancel_export" ), icons->get ( "menu/delete" ), [ this, rows, canCancel ]
	{
		for ( auto i = 0; i < rows.size (); ++i )
		{
			if ( ! canCancel ( rows[ i ] ) )
				continue;

			tuneExporter->removeEntry ( toQueueIndex ( rows[ i ] ) );
			UI::repaintCell ( this, rows[ i ], columnId::exportProgress );
		}

		save ();

	} ).setEnabled ( anySelected ( canCancel ) ) );

	UI::showMenuAtMouse ( m, *this );
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::paintCell ( juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected )
{
	if ( ! juce::isPositiveAndBelow ( rowNumber, getNumRows () ) )
		return;

	auto	b = juce::Rectangle<int> { width, height }.toFloat ().reduced ( 4.0f, 8.0f ).withTrimmedLeft ( 10.0f );

	g.setColour ( findColour ( UI::colors::textMuted ) );
	auto	progCol = findColour ( UI::colors::statusInfo );

	switch ( columnId )
	{
		case columnId::exportProgress:
		{
			const auto	queueIndex = toQueueIndex ( rowNumber );

			const auto	status = tuneExporter->getStatus ( queueIndex );
			auto		statusStr = tuneExporter->getStatusString ( queueIndex );

			auto	progress = -1.0f;
			// Add percentage if applicable
			switch ( status )
			{
				case TuneExporter::RENDERING:
					progress = renderProgress[ rowNumber ];
					break;

				case TuneExporter::APPLYING_FX:
					progress = renderProgress[ rowNumber ];
					break;

				case TuneExporter::SAVING:
					progress = renderProgress[ rowNumber ];
					break;

				case TuneExporter::COMPLETE:
					g.setColour ( findColour ( UI::colors::statusOk ) );
					break;

				case TuneExporter::CANCELED:
					g.setColour ( findColour ( UI::colors::statusWarning ) );
					break;

				case TuneExporter::PAUSED:
					progress = renderProgress[ rowNumber ];
					break;

				case TuneExporter::ERROR:
					g.setColour ( findColour ( UI::colors::statusError ) );
					break;
			}

			if ( progress >= 0.0f )
				statusStr += " " + juce::String ( int ( progress * 100.0f ) ) + "%";

			g.setFont ( UI::font ( UI::fonts::browser_text ) );
			g.drawText ( statusStr, b.removeFromTop ( b.getHeight () / ( progress >= 0.0f ? 2.0f : 1.0f ) ), juce::Justification::centredLeft );

			if ( progress >= 0.0f )
			{
				const auto	r = b.withSizeKeepingCentre ( b.getWidth (), 6.0f );
				auto		clip = GUI_RoundedClip ( g, r, r.getHeight () * 0.5f );

				g.fillAll ( findColour ( UI::colors::text ).withAlpha ( 0.25f ) );

				if ( progress >= 0.0f )
				{
					g.setColour ( progCol );
					g.fillRect ( r.withWidth ( r.getWidth () * progress ) );
				}
			}
		}
		break;

		default:
			GUI_ListBox::paintCell ( g, rowNumber, columnId, width, height, rowIsSelected );
	}
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::clear ()
{
	rowData.clear ();
	rowSubtune.clear ();
	renderProgress.clear ();
	rowFile.clear ();

	updateContent ();
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::addItem ( const std::string& tuneName, const TuneExporter::entry& queueEntry )
{
	// Unknown tune, no row, so no queue entry either
	if ( ! addNewEntry ( tuneName, queueEntry.subtune ) )
		return;

	tuneExporter->addEntry ( queueEntry );
	renderProgress.insert ( renderProgress.begin (), 0.0f );

	applyRetention ();
	updateContent ();
	save ();
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::update ()
{
	const juce::ScopedLock	sl ( tuneExporter->getLock () );

	// Get rendering progress
	for ( const auto idx : tuneExporter->findEntries ( { TuneExporter::RENDERING, TuneExporter::APPLYING_FX, TuneExporter::SAVING } ) )
	{
		const auto&	ent = tuneExporter->getEntry ( idx );
		const auto	row = toQueueIndex ( idx );

		auto	progress = renderProgress[ row ];
		switch ( ent.status )
		{
			case TuneExporter::RENDERING:
				progress = ent.renderProgressMs / float ( ent.lengthMs );
				break;

			case TuneExporter::APPLYING_FX:
				progress = ent.fxProgressMs / float ( ent.lengthMs );
				break;

			case TuneExporter::SAVING:
				progress = ent.savingPercent * 0.01f;
				break;
		}

		if ( progress != renderProgress[ row ] )
		{
			renderProgress[ row ] = progress;
			UI::repaintCell ( this, row, columnId::exportProgress );
		}
	}
}
//-----------------------------------------------------------------------------

bool GUI_ExportItems::removeEntry ( const int row )
{
	// The row disappears only when the exporter entry is really gone
	// (finished and no thread still touching it)
	if ( ! tuneExporter->eraseEntry ( toQueueIndex ( row ) ) )
		return false;

	rowData.erase ( rowData.begin () + row );
	rowSubtune.erase ( rowSubtune.begin () + row );
	renderProgress.erase ( renderProgress.begin () + row );
	rowFile.erase ( rowFile.begin () + row );

	updateContent ();

	return true;
}
//-----------------------------------------------------------------------------

bool GUI_ExportItems::addNewEntry ( const std::string& tune, const int tuneNo )
{
	auto	ent = db::findDatabaseEntry ( tune );
	if ( ! ent )
		return false;

	rowData.insert ( rowData.begin (), ent );
	rowSubtune.insert ( rowSubtune.begin (), int16_t ( tuneNo ) );
	rowFile.insert ( rowFile.begin (), std::string ( ent->file ) );

	return true;
}
//-----------------------------------------------------------------------------

// The oldest entries sit at the bottom; live entries are always young, so the
// walk stops long before reaching them
void GUI_ExportItems::applyRetention ()
{
	const auto	cutoff = ( juce::Time::getCurrentTime () - juce::RelativeTime::days ( maxRetainedAgeDays ) ).toMilliseconds ();

	while ( getNumRows () > maxRetainedItems )
	{
		const auto	row = getNumRows () - 1;

		if ( tuneExporter->getEntry ( toQueueIndex ( row ) ).date >= cutoff )
			break;

		if ( ! removeEntry ( row ) )
			break;
	}
}
//-----------------------------------------------------------------------------

// Live entries stay: eraseEntry refuses anything a thread still works on
void GUI_ExportItems::clearAll ()
{
	for ( auto row = getNumRows () - 1; row >= 0; --row )
		removeEntry ( row );

	deselectAllRows ();
	save ();

	// Rows may vanish without new ones taking their place, and the placeholder
	// spans the full component: the partial row repaint doesn't cover it
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::clearOlderThan ( const double days )
{
	const auto	cutoff = ( juce::Time::getCurrentTime () - juce::RelativeTime::days ( days ) ).toMilliseconds ();

	while ( getNumRows () > 0 )
	{
		const auto	row = getNumRows () - 1;

		if ( tuneExporter->getEntry ( toQueueIndex ( row ) ).date >= cutoff )
			break;

		if ( ! removeEntry ( row ) )
			break;
	}

	deselectAllRows ();
	save ();
	repaint ();
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::reAddCanceled ()
{
	for ( auto row = 0; row < getNumRows (); ++row )
		reAddRow ( row );
}
//-----------------------------------------------------------------------------

// Revives a canceled row in place, Chrome-downloads style: it keeps its list
// position and date, but renders with the current settings, template and data
void GUI_ExportItems::reAddRow ( const int row )
{
	const auto	queueIndex = toQueueIndex ( row );

	if ( tuneExporter->getStatus ( queueIndex ) != TuneExporter::CANCELED )
		return;

	const auto	dbEnt = rowData[ row ];
	if ( ! dbEnt )	// The tune is gone, there is nothing to render
		return;

	const juce::SharedResourcePointer<Preferences>	preferences;
	const auto	quality = preferences->get<juce::String> ( "player/quality" );

	const auto	saveName = exportname::makeExportPath ( *dbEnt, rowSubtune[ row ], quality.toStdString () );
	if ( saveName.empty () )
		return;

	const auto	[ lengthMs, fadeMs, ebuGain, filterUsed, startMs ] = SID::getRenderInfo ( rowFile[ row ], rowSubtune[ row ] );

	auto&	ent = tuneExporter->getEntry ( queueIndex );

	ent.exportFilename = saveName;
	ent.fxMode = GUI_Export::fxModeForQuality ( quality );
	ent.lengthMs = int ( lengthMs );
	ent.fadeOutMs = int ( fadeMs );
	ent.startMs = int ( startMs );
	ent.ebuGain = ebuGain;
	ent.useFilter = filterUsed;
	ent.normalize = preferences->get<bool> ( "export/normalize" );

	tuneExporter->reAddEntry ( queueIndex );

	UI::repaintCell ( this, row, columnId::exportProgress );
}
//-----------------------------------------------------------------------------

bool GUI_ExportItems::hasCanceled () const
{
	return ! tuneExporter->findEntries ( { TuneExporter::CANCELED } ).empty ();
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::save ()
{
	const juce::ScopedLock	sl ( tuneExporter->getLock () );

	std::string	list = csvHeader;

	for ( auto row = 0; row < getNumRows (); ++row )
	{
		const auto&	ent = tuneExporter->getEntry ( toQueueIndex ( row ) );

		// Live work cannot resume across a restart, it reloads as canceled
		auto	status = ent.status.load ();
		if ( status != TuneExporter::COMPLETE && status != TuneExporter::ERROR )
			status = TuneExporter::CANCELED;

		// Only a completed export names a real file on disk; everything else
		// re-queues as a fresh export and gets a fresh name then
		const auto	exportName = status == TuneExporter::COMPLETE ? juce::String ( ent.exportFilename ) : juce::String ();

		// The tune cell speaks the playlist dialect: a bare name means the
		// default subtune
		auto	tune = juce::String ( rowFile[ row ] );
		if ( ! rowData[ row ] || ent.subtune != rowData[ row ]->startTune )
			tune += "," + juce::String ( ent.subtune );

		juce::StringArray	fields;

		fields.add ( tune.quoted () );
		fields.add ( TuneExporter::statusLabel ( status ) );
		fields.add ( juce::Time ( ent.date ).toISO8601 ( true ) );
		fields.add ( exportName.quoted () );

		list += fields.joinIntoString ( "," ).toStdString ();
		list += '\n';
	}

	fileutils::replaceFile ( filepaths::getExportListPath (), list.c_str (), list.size () );
}
//-----------------------------------------------------------------------------

void GUI_ExportItems::load ()
{
	auto	file = filepaths::getExportListPath ();

	if ( ! file.existsAsFile () )
		return;

	libsidplayEZ::TinyCSV	csv;
	const auto	numEntries = csv.parseCSV ( file.loadFileAsString ().toStdString () );

	if ( const auto& err = csv.getError (); ! err.empty () )
		Z_ERR ( "Export list: " << err );

	// The file is newest-first, the queue appends oldest-first
	for ( auto i = numEntries - 1; i >= 0; --i )
	{
		// A bare tune name means the default subtune, like in the playlists
		const auto	[ tuneKey, fileSubtune ] = SID::parseTuneName ( csv.get ( i, "tune", "" ) );
		if ( tuneKey.empty () )
			continue;

		const auto	dbEnt = db::findDatabaseEntry ( tuneKey );

		TuneExporter::entry	ent;

		ent.subtune = fileSubtune;
		if ( ent.subtune == 0 && dbEnt )
			ent.subtune = dbEnt->startTune;
		ent.date = juce::Time::fromISO8601 ( juce::String ( csv.get ( i, "date", "" ) ) ).toMilliseconds ();

		// The render parameters always come fresh from the database, so an
		// entry re-queued after a data update renders with corrected values
		const auto	[ lengthMs, fadeMs, ebuGain, filterUsed, startMs ] = SID::getRenderInfo ( tuneKey, ent.subtune );

		ent.lengthMs = int ( lengthMs );
		ent.fadeOutMs = int ( fadeMs );
		ent.startMs = int ( startMs );
		ent.ebuGain = ebuGain;
		ent.useFilter = filterUsed;

		// The tune's full path is rebuilt against the current roots; a tune
		// that is gone resolves to nothing, but its record stays as a red row
		ent.tuneFilename = filepaths::resolveTune ( tuneKey ).toLoadable ().toStdString ();

		// The record of the file a completed export wrote
		ent.exportFilename = csv.get ( i, "exportFilename", "" );

		// Only finished entries are records, anything else in the file is stale
		auto	status = TuneExporter::statusFromLabel ( csv.get ( i, "status", "" ) );
		if ( status != TuneExporter::COMPLETE && status != TuneExporter::ERROR )
			status = TuneExporter::CANCELED;
		ent.status = status;

		rowData.insert ( rowData.begin (), dbEnt );
		rowSubtune.insert ( rowSubtune.begin (), int16_t ( ent.subtune ) );
		rowFile.insert ( rowFile.begin (), tuneKey );
		renderProgress.insert ( renderProgress.begin (), 0.0f );

		tuneExporter->addEntry ( ent );
	}

	const auto	before = getNumRows ();
	applyRetention ();
	if ( getNumRows () != before )
		save ();

	updateContent ();
}
//-------------------------------------------------------------------------------------------------

void GUI_ExportItems::refreshRowData ()
{
	for ( auto i = 0u; i < rowFile.size (); ++i )
		rowData[ i ] = db::findDatabaseEntry ( rowFile[ i ] );

	updateContent ();
	repaint ();
}
//-------------------------------------------------------------------------------------------------

juce::String GUI_ExportItems::getMissingRowText ( const int rowNumber ) const
{
	return filepaths::stripLocationMarker ( rowFile[ rowNumber ] );
}
//-------------------------------------------------------------------------------------------------
