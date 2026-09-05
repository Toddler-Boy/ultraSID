#include "App/TuneExporter.h"
#include "Helpers/Messages.h"

#include "GUI_ultraSID.h"

//-----------------------------------------------------------------------------

void GUI_ultraSID::registerExportActions ()
{
	router.on<msg::ExportTune> ( [ this ] ( const auto& e )
	{
		for ( const auto& item : e.tunes )
			mainScreen.pages.addExportItem ( getFullFilename ( item.upToLastOccurrenceOf ( ",", false, false ) ), item.toStdString () );
	} );

	// User actions float their count delta off the pill, worker progress
	// only updates the pill itself
	const auto	updatePill = [ this ] ( const bool user )
	{
		const juce::SharedResourcePointer<TuneExporter>	tuneExporter;

		const auto	work = tuneExporter->getNumWorkEntries ();
		const auto	previous = std::exchange ( lastExportWork, work );

		mainScreen.sidebarLeft.setExportBadgeCounts ( work, tuneExporter->getNumErrorEntries () );

		if ( user )
			badgeOverlay.spawn ( badgeOverlay.getLocalPoint ( nullptr, mainScreen.sidebarLeft.exportScreenAnchor () ), work - previous );
	};

	router.on<msg::UpdateExportBadge> ( [ updatePill ]		{	updatePill ( false );	} );
	router.on<msg::UpdateExportBadgeUser> ( [ updatePill ]	{	updatePill ( true );	} );

	router.on<msg::ExportEntryStatusUpdate> ( [ this ] ( const auto& e )
	{
		mainScreen.pages.repaintExportCell ( e.index );
	} );
}
//-----------------------------------------------------------------------------
