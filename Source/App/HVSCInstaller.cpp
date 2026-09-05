#include <fmt/format.h>

#include "HVSCInstaller.h"

#include "ultra-shared/Config/ZipFolder.h"

#include "Config/HVSCSource.h"
#include "Database/HVSCUpdater/HVSCUpdater.h"
#include "Helpers/Messages.h"
#include "Helpers/Unarchiver.h"

//-----------------------------------------------------------------------------

// The CDN archives: the full collection for fresh installs (and installs too
// old to patch) and one update archive per release; {} = version slot
constexpr auto	fullArchiveURL = "https://cdn.ultrasid.com/HVSC/HVSC_{}-all-of-them.7z";
constexpr auto	updateArchiveURL = "https://cdn.ultrasid.com/HVSC/HVSC_Update_{}.7z";

// HVSC content: .sid files stay under 64 KB, the DOCUMENTS texts a few MB,
// and dense C64 data compresses ~2-4:1
constexpr Unarchiver::Limits	hvscLimits { .maxEntrySize = 16ull << 20, .maxTotalRatio = 8 };

//-----------------------------------------------------------------------------

HVSCInstaller::HVSCInstaller ()
	: juce::Thread ( "Archive extraction thread" )
{
	downloader.setProgressInterval ( 1000 / 30 );
}
//-----------------------------------------------------------------------------

HVSCInstaller::~HVSCInstaller ()
{
	canceler.stopThread ( -1 );
	stopThread ( -1 );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::postAsync ( std::function<void ( HVSCInstaller& )> fn )
{
	juce::MessageManager::callAsync ( [ weak = juce::WeakReference<HVSCInstaller> ( this ), fn = std::move ( fn ) ]
	{
		if ( weak != nullptr )
			fn ( *weak );
	} );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::report ( const Status status, const juce::String& message )
{
	if ( onStatus )
		onStatus ( status, message );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::reportHVSCStatus ()
{
	if ( ! installState->hvsc.status.empty () )
	{
		report ( Status::error, installState->hvsc.status );
		return;
	}

	if ( installState->hvsc.versionInstalled < 0 )
	{
		report ( Status::error, "Not installed or incomplete" );
		return;
	}

	if ( installState->hvsc.versionInstalled == 0 )
	{
		report ( Status::ok, "Checking version..." );
		return;
	}

	// The installed version, measured against the release this build ships for
	juce::String	msg = "Installed version " + juce::String ( installState->hvsc.versionInstalled );

	if ( installState->hvsc.needsUpdate () )
	{
		msg += " / Update to version " + juce::String ( installState->hvsc.targetVersion ) + " available";
		report ( Status::warning, msg );
		return;
	}

	if ( installState->hvsc.versionInstalled > installState->hvsc.targetVersion )
	{
		// A hand-updated collection: its newer tunes stay outside the shipped database
		msg += " / Newer than this ultraSID version expects (" + juce::String ( installState->hvsc.targetVersion ) + ")";
		report ( Status::warning, msg );
		return;
	}

	report ( Status::ok, msg + " / Up to date" );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::reportDatabaseStatus ()
{
	if ( ! installState->database.status.empty () )
	{
		report ( Status::error, installState->database.status );
		return;
	}

	if ( installState->database.versionInstalled == 0 )
	{
		report ( Status::error, "The tune database is unreadable. Please download and install ultraSID again." );
		return;
	}

	if ( installState->database.versionInstalled < 0 )
	{
		report ( Status::ok, "Checking database version..." );
		return;
	}
}
//-----------------------------------------------------------------------------

void HVSCInstaller::downloadUpdate ()
{
	installState->server.reset ();
	installState->hvsc.reset ();

	const auto	firstVersion = installState->hvsc.versionInstalled + 1;
	const auto	lastVersion = installState->hvsc.targetVersion;

	if ( lastVersion > firstVersion )
		installState->progress.reset ( {	fmt::format ( "Downloading HVSC updates {}-{}...", firstVersion, lastVersion ),
											fmt::format ( "Extracting HVSC updates {}-{}...", firstVersion, lastVersion ),
											fmt::format ( "Applying HVSC updates {}-{}...", firstVersion, lastVersion )
									   } );
	else
		installState->progress.reset ( {	fmt::format ( "Downloading HVSC {} update...", firstVersion ),
											fmt::format ( "Extracting HVSC {} update...", firstVersion ),
											fmt::format ( "Applying HVSC {} update...", firstVersion )
										} );

	// Skip an update the collection already carries (the .hvs lands in
	// DOCUMENTS only after a zero-error run)
	{
		if ( hvscsource::exists ( "DOCUMENTS/Update" + juce::String ( firstVersion ) + ".hvs" ) )
		{
			Z_ERR ( "HVSC update " << firstVersion << " already downloaded!" );

			installState->hvsc.status = "HVSC update already applied?";
			reportHVSCStatus ();
			return;
		}
	}

	downloadedUpdates.clear ();
	downloadNextUpdate ( firstVersion );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::downloadNextUpdate ( const int version )
{
	const auto	url = formatURL ( updateArchiveURL, version, "update archive", "Update URL invalid" );
	if ( url.empty () )
		return;

	chainIndex = int ( downloadedUpdates.size () );
	chainCount = std::max ( 1, installState->hvsc.targetVersion - installState->hvsc.versionInstalled );

	startDownload ( url, [ this, version ] ( const juce::MemoryBlock& data )
	{
		downloadedUpdates.push_back ( data );

		if ( version < installState->hvsc.targetVersion )
		{
			downloadNextUpdate ( version + 1 );
			return;
		}

		installState->progress.setState ( 1 );

		currentTask = task::update;
		startThread ( juce::Thread::Priority::low );
	} );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::downloadFull ()
{
	installState->server.reset ();
	installState->hvsc.reset ();
	installState->progress.reset ( {
		fmt::format ( "Downloading full HVSC {}...", installState->hvsc.targetVersion ),
		fmt::format ( "Extracting HVSC {}...", installState->hvsc.targetVersion ) } );

	// Skip a version the collection already carries (an interrupted install
	// leaves the marker behind but must be repairable)
	{
		if ( hvscsource::exists ( "DOCUMENTS/Update" + juce::String ( installState->hvsc.targetVersion ) + ".hvs" )
			 && ! settings->get<bool> ( "hvsc/install-in-progress" ) )
		{
			Z_ERR ( "HVSC " << installState->hvsc.targetVersion << " already downloaded!" );

			installState->hvsc.status = "HVSC already installed";
			reportHVSCStatus ();
			return;
		}
	}

	// Download full archive (URL first: a bad template must not set the
	// install-in-progress flag below)
	const auto	url = formatURL ( fullArchiveURL, installState->hvsc.targetVersion, "full archive", "Install URL invalid" );
	if ( url.empty () )
		return;

	// Persisted right away, a killed install must leave the flag behind
	settings->set ( "hvsc/install-in-progress", true );
	settings->save ();

	chainIndex = 0;
	chainCount = 1;

	startDownload ( url, [ this ] ( const juce::MemoryBlock& data )
	{
		extractFull ( data );
	} );
}
//-----------------------------------------------------------------------------

std::string HVSCInstaller::formatURL ( const std::string& tmpl, const int version, const char* what, const char* statusOnError )
{
	try
	{
		return fmt::vformat ( tmpl, fmt::make_format_args ( version ) );
	}
	catch ( const fmt::format_error& )
	{
		Z_ERR ( "Invalid " << what << " URL template: " << tmpl );
		installState->hvsc.status = statusOnError;
		reportHVSCStatus ();
		return {};
	}
}
//-----------------------------------------------------------------------------

void HVSCInstaller::startDownload ( const std::string& url, std::function<void ( const juce::MemoryBlock& )> onData )
{
	installState->hvsc.downloadId = downloader.startAsyncDownload ( juce::URL ( url ), [ url, onData = std::move ( onData ), this ] ( gin::DownloadManager::DownloadResult res )
	{
		installState->hvsc.downloadId = 0;
		installState->server.setState ( res.httpCode );

		if ( ! res.ok )
		{
			Z_ERR ( "Couldn't download " << url << " - HTTP/" << res.httpCode );

			// Reported as an error, so the install screens fall back to their start page
			installState->hvsc.status = "Download failed";
			if ( ! installState->server.response.empty () )
				installState->hvsc.status += " (" + installState->server.response + ")";

			reportHVSCStatus ();
			return;
		}

		onData ( res.data );
	},
	[ this ] ( juce::int64 current, juce::int64 total, juce::int64 )
	{
		// Download progress bar (int is plenty, the full HVSC 7z is well under
		// 100 MB); an update chain spreads its files across the one bar
		installState->progress.maxFiles = int ( total );
		installState->progress.currentFiles = int ( current );
		installState->progress[ 0 ] = ( float ( chainIndex ) + float ( current ) / float ( total ) ) / float ( chainCount );
	} );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::extractFull ( const juce::MemoryBlock& data )
{
	downloadedData = data;
	currentTask = task::full;
	installCanceled = false;

	startThread ( juce::Thread::Priority::low );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::cancelFullInstall ()
{
	// A cancel is already running
	if ( canceler.isThreadRunning () )
		return;

	installCanceled = true;

	// Show the cancelation UI
	if ( onCanceling )
		onCanceling ( true );

	// gin's DownloadManager is message-thread-only; canceling synchronously
	// also means no pending completion can start an extraction afterwards
	downloader.cancelDownload ( installState->hvsc.downloadId );
	installState->hvsc.downloadId = 0;

	// The slow part (stopping extraction, deleting the partial tree) runs on
	// the owned canceler thread
	canceler.startThread ();
}
//-----------------------------------------------------------------------------

// Runs on the canceler thread
void HVSCInstaller::cancelWork ()
{
	// Stop a running extraction; a zip install is atomic and leaves no
	// partial files
	stopThread ( -1 );

	if ( ! hvscRoot.hasFileExtension ( "zip" ) )
	{
		// Delete the partial files one by one with an exit hatch, quitting must
		// not block behind a 50k-file tree
		for ( const auto& entry : juce::RangedDirectoryIterator ( hvscRoot, true, "*", juce::File::findFiles ) )
		{
			if ( juce::Thread::currentThreadShouldExit () )
				return;

			entry.getFile ().deleteFile ();
		}

		// Only the folder skeleton is left, this is quick
		if ( ! juce::Thread::currentThreadShouldExit () )
			hvscRoot.deleteRecursively ();
	}

	// Switch back to the onboarding page
	postAsync ( [] ( auto& self )
	{
		if ( self.onCanceled )
			self.onCanceled ( true );
	} );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::cancelUpdate ()
{
	// Everything here is quick and message-thread-only, no thread needed
	// (see cancelFullInstall regarding gin's DownloadManager)
	if ( onCanceling )
		onCanceling ( false );

	downloader.cancelDownload ( installState->hvsc.downloadId );
	installState->hvsc.downloadId = 0;

	// Switch back to update HVSC-page
	if ( onCanceled )
		onCanceled ( false );
}
//-----------------------------------------------------------------------------

void HVSCInstaller::run ()
{
	const auto	current = currentTask;
	currentTask = task::none;

	//
	// HVSC update extraction and update
	//
	if ( current == task::update )
	{
		const auto	firstVersion = installState->hvsc.versionInstalled + 1;
		const auto	zipMode = hvscRoot.hasFileExtension ( "zip" );
		auto*		zip = hvscsource::archive ();

		// The whole chain stages in memory and commits once; any failure
		// discards everything, the collection stays at its current version
		auto fail = [ this, zip, zipMode ] ( const char* status )
		{
			if ( zipMode )
				zip->discardPendingChanges ();

			postAsync ( [ status ] ( auto& self ) {	self.installState->hvsc.status = status; self.reportHVSCStatus ();	} );
		};

		if ( zipMode && zip == nullptr )
		{
			Z_ERR ( "HVSC update: zip collection not attached" );
			postAsync ( [] ( auto& self ) {	self.installState->hvsc.status = "HVSC update failed extraction"; self.reportHVSCStatus ();	} );
			return;
		}

		// Clear update-folder leftovers from an interrupted run
		if ( zipMode )
		{
			zip->discardPendingChanges ();
			zip->removeFolder ( hvscsource::archivePrefix () + "update" );
		}
		else
		{
			hvscRoot.getChildFile ( "update" ).deleteRecursively ();
		}

		const auto	tree = zipMode
						 ? std::unique_ptr<HVSCTree> ( std::make_unique<ZipTree> ( *zip, hvscsource::archivePrefix () ) )
						 : std::make_unique<FolderTree> ( hvscRoot );

		// Deliberately not abortable (no thread passed, unlike the full
		// install): extraction + scripts finish within seconds, and
		// interrupting a script would half-apply the update
		for ( auto i = 0; i < int ( downloadedUpdates.size () ); ++i )
		{
			const auto	version = firstVersion + i;

			installState->progress.setState ( 1 );

			const auto	numFilesExtracted = zipMode
											? Unarchiver::extractArchiveInto ( *zip, hvscsource::archivePrefix (), downloadedUpdates[ size_t ( i ) ], installState->progress[ 1 ], installState->progress.currentFiles, installState->progress.maxFiles, hvscLimits )
											: Unarchiver::extractArchive ( hvscRoot.getFullPathName ().toStdString (), downloadedUpdates[ size_t ( i ) ], installState->progress[ 1 ], installState->progress.currentFiles, installState->progress.maxFiles, nullptr, hvscLimits );

			if ( numFilesExtracted < 0 )
			{
				Z_ERR ( "HVSC update " << version << " failed during extraction into " << hvscRoot.getFullPathName () );
				fail ( "HVSC update failed extraction" );
				return;
			}

			installState->progress.setState ( 2 );

			const auto	updateErrors = HVSCUpdater ().update ( *tree, version, installState->progress[ 2 ], installState->progress.currentFiles, installState->progress.maxFiles );
			if ( updateErrors > 0 )
			{
				Z_ERR ( "HVSC update " << version << " failed with " << updateErrors << " errors" );
				fail ( "HVSC update-script error" );
				return;
			}
		}

		if ( ! tree->finish () )
		{
			Z_ERR ( "HVSC update chain failed writing " << hvscRoot.getFullPathName () );
			fail ( "HVSC update write error" );
			return;
		}

		downloadedUpdates.clear ();

		postAsync ( [] ( auto& self ) {	if ( self.onInstallFinished ) self.onInstallFinished ();	} );

		return;
	}

	//
	// HVSC full extraction
	//
	if ( current == task::full )
	{
		installState->progress.setState ( 1 );

		auto	numFilesExtracted = 0;

		if ( hvscRoot.hasFileExtension ( "zip" ) )
		{
			// Converts into the single-archive collection, replacing atomically
			numFilesExtracted = Unarchiver::convertArchiveToZip ( hvscRoot, downloadedData, installState->progress[ 1 ], installState->progress.currentFiles, installState->progress.maxFiles, this, hvscLimits );
		}
		else
		{
			auto	dstPath = hvscRoot.getFullPathName ().replaceCharacter ( '\\', '/' );

			if ( dstPath.endsWithIgnoreCase ( "/C64Music" ) )
			{
				hvscRoot.deleteRecursively ();
				dstPath = hvscRoot.getParentDirectory ().getFullPathName ();
			}

			numFilesExtracted = Unarchiver::extractArchive ( dstPath.toStdString (), downloadedData, installState->progress[ 1 ], installState->progress.currentFiles, installState->progress.maxFiles, this, hvscLimits );
		}

		if ( numFilesExtracted < 0 )
		{
			Z_ERR ( "HVSC " << installState->hvsc.targetVersion << " failed during extraction into " << hvscRoot.getFullPathName () );
			postAsync ( [] ( auto& self )
			{
				// A canceled install fails extraction too and reports
				// through onCanceled instead
				if ( self.installCanceled )
					return;

				self.installState->hvsc.status = "HVSC install error";
				self.reportHVSCStatus ();
			} );
			return;
		}

		if ( ! threadShouldExit () )
			postAsync ( [] ( auto& self )
			{
				// A cancel may have landed between posting this and running it
				if ( self.installCanceled )
					return;

				self.settings->set ( "hvsc/install-in-progress", false );
				self.settings->save ();

				if ( self.onInstallFinished )
					self.onInstallFinished ();
			} );

		return;
	}
}
//-----------------------------------------------------------------------------
