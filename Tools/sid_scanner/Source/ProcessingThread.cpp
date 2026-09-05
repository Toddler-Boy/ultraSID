#include <fmt/format.h>
#include <unordered_set>

#include "ProcessingThread.h"

#include "ultra-shared/Config/YamlFile.h"
#include "ultra-shared/Helpers/FileUtils.h"
#include "ultra-shared/Helpers/Regex.h"

#include "Database.h"
#include "DatabaseBuilder.h"
#include "sid_scanner.h"

//-----------------------------------------------------------------------------

// Fired after a finished scan that ran for at least half an hour: sends a
// completion mail through curl (stock on Windows) when Data/mail.yml holds
// the smtp configuration; no config file, no mail
static void sendCompletionMail ( const int measured, const int64_t elapsedMs )
{
	constexpr auto	minRuntimeMs = int64_t ( 30 ) * 60 * 1000;

	if ( elapsedMs < minRuntimeMs )
		return;

	const auto	configFile = scannerDataFile ( "mail.yml" );
	if ( ! configFile.existsAsFile () )
		return;

	const YamlFile	config ( { { "smtp", "server", std::string () },
							   { "smtp", "user", std::string () },
							   { "smtp", "password", std::string () },
							   { "mail", "from", std::string () },
							   { "mail", "to", std::string () } }, configFile, true );

	const auto	server = config.get<juce::String> ( "smtp/server" );
	const auto	from = config.get<juce::String> ( "mail/from" );
	const auto	to = config.get<juce::String> ( "mail/to" );

	if ( server.isEmpty () || from.isEmpty () || to.isEmpty () )
	{
		Z_ERR ( "mail.yml is missing smtp/server, mail/from or mail/to" );
		return;
	}

	const auto	minutes = int ( elapsedMs / 60'000 );
	const auto	message = "From: " + from + "\r\nTo: " + to
						+ "\r\nSubject: sid_scanner finished\r\n\r\n"
						+ juce::String ( measured ) + " tunes measured in "
						+ juce::String ( minutes / 60 ) + ":" + juce::String ( minutes % 60 ).paddedLeft ( '0', 2 ) + " hours.\r\n";

	const auto	messageFile = juce::File::createTempFile ( "sid_scanner_mail" );
	messageFile.replaceWithText ( message );

	// Port 465 is implicit TLS from the first byte (smtps), everything else
	// starts plain and upgrades via STARTTLS where offered
	const auto	url = server.endsWith ( ":465" ) ? "smtps://" + server : "smtp://" + server;

	juce::StringArray	args { "curl", "--silent", "--show-error", "--ssl",
							   "--url", url,
							   "--mail-from", from, "--mail-rcpt", to,
							   "--upload-file", messageFile.getFullPathName () };

	if ( const auto user = config.get<juce::String> ( "smtp/user" ); user.isNotEmpty () )
	{
		args.add ( "--user" );
		args.add ( user + ":" + config.get<juce::String> ( "smtp/password" ) );
	}

	juce::ChildProcess	curl;
	if ( ! curl.start ( args ) )
	{
		Z_ERR ( "curl did not start, no completion mail sent" );
		messageFile.deleteFile ();
		return;
	}

	curl.waitForProcessToFinish ( 30'000 );
	const auto	output = curl.readAllProcessOutput ().trim ();
	messageFile.deleteFile ();

	if ( curl.getExitCode () != 0 )
	{
		Z_ERR ( "curl failed sending the completion mail: " << output );
	}
	else
	{
		Z_LOG ( "Completion mail sent to " << to );
	}
}

//-----------------------------------------------------------------------------

ProcessingThread::ProcessingThread () : juce::Thread ( "ultraSID_tool processing" )
{
}
//-----------------------------------------------------------------------------

ProcessingThread::~ProcessingThread ()
{
	stopThread ( -1 );
}
//-----------------------------------------------------------------------------

int ProcessingThread::getNumQueueEntries () const
{
	const juce::CriticalSection::ScopedLockType	lock ( queueLock );

	return static_cast<int> ( queue.size () );
}
//-----------------------------------------------------------------------------

const ProcessingThread::QueueEntry* ProcessingThread::getQueueEntry ( const int index ) const
{
	const juce::CriticalSection::ScopedLockType	lock ( queueLock );

	return juce::isPositiveAndBelow ( index, static_cast<int> ( queue.size () ) ) ? queue[ static_cast<size_t> ( index ) ].get () : nullptr;
}
//-----------------------------------------------------------------------------

// Escape regex metacharacters in a literal tune path; '/' stays untouched,
// the pattern queue escapes those itself
static juce::String escapePatternLiteral ( const juce::String& text )
{
	juce::String	out;

	for ( const auto c : text )
	{
		if ( juce::String ( ".^$|()[]{}*+?\\" ).containsChar ( c ) )
			out += '\\';
		out += c;
	}

	return out;
}
//-----------------------------------------------------------------------------

void ProcessingThread::addPattern ( const juce::String& pattern, const bool force6581, const bool force8580 )
{
	// @<file>: a list file with one tune path per line ('#' lines are
	// comments); every line queues as its own anchored exact-match pattern
	if ( pattern.startsWithChar ( '@' ) )
	{
		const juce::File	listFile ( pattern.substring ( 1 ).unquoted ().trim () );

		if ( ! listFile.existsAsFile () )
		{
			sendActionMessage ( "error List file not found: \"" + listFile.getFullPathName () + "\"" );
			return;
		}

		juce::StringArray	lines;
		listFile.readLines ( lines );

		for ( const auto& raw : lines )
		{
			const auto	line = raw.trim ().replace ( "\\", "/" );
			if ( line.isEmpty () || line.startsWithChar ( '#' ) )
				continue;

			addPattern ( "^" + escapePatternLiteral ( line ) + "$", force6581, force8580 );
		}

		return;
	}

	{
		const juce::CriticalSection::ScopedLockType	lock ( patternLock );

		pendingPatterns.push_back ( { pattern, force6581, force8580 } );
	}

	notify ();
}
//-----------------------------------------------------------------------------

void ProcessingThread::run ()
{
	const auto	hvscPathFile = ultraSIDHVSCPath ();

	if ( ! hvscsource::setRoot ( hvscPathFile ) )
	{
		sendActionMessage ( "error ultraSID's HVSC path isn't valid: \"" + hvscPathFile.getFullPathName () + "\"" );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}
	tunepatches::load ( dataRoot ().getChildFile ( "Databases/tune-patches.txt" ).loadFileAsString () );

	const auto&	hvscPath = hvscPathFile.getFullPathName ();

	// Load SID player config, to identify the player algorithm used for each SID
	const auto	sididCfg = dataRoot ().getChildFile ( "sidid.cfg" );
	if ( ! sididCfg.existsAsFile () )
	{
		sendActionMessage ( "error Can't find ultraSID's \"Data/sidid.cfg\"" );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}
	const auto	sididCfgPath = sididCfg.getFullPathName ().toRawUTF8 ();

	// Load various profiles and overrides
	auto loadCSV = [ this ] ( const juce::String& name ) -> std::string
	{
		const auto	csv = dataRoot ().getChildFile ( "Databases" ).getChildFile ( name );
		if ( ! csv.existsAsFile () )
		{
			sendActionMessage ( "error Can't find ultraSID's \"Data/Databases/" + name + "\"" );
			return {};
		}
		return csv.loadFileAsString ().toStdString ();
	};

	const auto	chipProfilesCSV = loadCSV ( "chip-profiles.csv" );
	if ( chipProfilesCSV.empty () )
	{
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	const auto	audioProfilesCSV = loadCSV ( "audio-profiles.csv" );
	if ( audioProfilesCSV.empty () )
	{
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	const auto	tuneOverridesCSV = loadCSV ( "tune-overrides.csv" );
	if ( tuneOverridesCSV.empty () )
	{
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	const auto	digiPlayersCSV = loadCSV ( "digi-players.csv" );
	if ( digiPlayersCSV.empty () )
	{
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	const auto	digiTunesCSV = loadCSV ( "digi-tunes.csv" );
	if ( digiTunesCSV.empty () )
	{
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	//
	// Parse all configuration once; every player re-uses the same shared data
	//
	auto	sharedConfig = std::make_shared<libsidplayEZ::SharedPlayerConfig> ();
	if ( ! sharedConfig->loadSidIDConfig ( sididCfgPath ) )
	{
		sendActionMessage ( "error Failed to parse \"Data/sidid.cfg\"" );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	// The loaders report the first unusable cell; a broken CSV means broken
	// measurements, never scan with one
	if ( const auto err = sharedConfig->loadChipProfiles ( chipProfilesCSV ); ! err.empty () )
	{
		sendActionMessage ( "error chip-profiles.csv: " + juce::String ( err ) );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	if ( const auto err = sharedConfig->loadAudioProfiles ( audioProfilesCSV ); ! err.empty () )
	{
		sendActionMessage ( "error audio-profiles.csv: " + juce::String ( err ) );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	if ( const auto err = sharedConfig->loadTuneOverrides ( tuneOverridesCSV ); ! err.empty () )
	{
		sendActionMessage ( "error tune-overrides.csv: " + juce::String ( err ) );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	if ( const auto err = sharedConfig->loadDigiPlayers ( digiPlayersCSV ); ! err.empty () )
	{
		sendActionMessage ( "error digi-players.csv: " + juce::String ( err ) );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	if ( const auto err = sharedConfig->loadDigiTunes ( digiTunesCSV ); ! err.empty () )
	{
		sendActionMessage ( "error digi-tunes.csv: " + juce::String ( err ) );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	// The Exotic-tunes addendum is part of the corpus, a copy without it would
	// silently scan less
	if ( ! dataRoot ().getChildFile ( "Databases/Songlengths-addendum.md5" ).existsAsFile () )
	{
		sendActionMessage ( "error Can't find ultraSID's \"Data/Databases/Songlengths-addendum.md5\"" );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	//
	// Parse command line: optional initial pattern(s), -f to re-measure, --batch
	// to build the database once the queue has drained even if nothing rendered
	//
	const auto	args = juce::JUCEApplicationBase::getCommandLineParameterArray ();

	const auto	batch = args.contains ( "--batch" );

	auto	cliForce = false;
	for ( const auto& arg : args )
		if ( arg.startsWithChar ( '-' ) && arg != "--batch" )
			cliForce = cliForce || arg.containsChar ( 'f' );

	for ( const auto& arg : args )
		if ( ! arg.startsWithChar ( '-' ) )
			addPattern ( arg, cliForce );

	//
	// Load lengths and LUFS databases
	//
	Database	db;

	db.setSharedConfig ( sharedConfig );
	db.attach ();
	if ( db.hvscVersion < Database::minHVSCVersion )
	{
		sendActionMessage ( "error HVSC outdated (version " + juce::String ( db.hvscVersion ) + ", minimum should be " + juce::String ( Database::minHVSCVersion ) + ")" );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	if ( db.db.empty () )
	{
		sendActionMessage ( "error Length database missing" );
		juce::JUCEApplication::getInstance ()->setApplicationReturnValue ( 20 );
		return;
	}

	sendActionMessage ( "hvsc " + hvscPath + "|" + juce::String ( db.hvscVersion ) + "|" + juce::String ( static_cast<int> ( db.db.size () ) ) );

	//
	// Process patterns as they come in, queueing all matching tunes
	//
	auto	cpuNum = juce::SystemStats::getNumCpus ();

	juce::ThreadPool		tp = { cpuNum, 0, juce::Thread::Priority::low };
	juce::CriticalSection	csPrinter;
	int						finishedCnt = 0;

	// Per-run totals for the drain detection and the completion mail; unlike
	// finishedCnt these survive the periodic ten-tune flush
	int			measuredTotal = 0;
	int64_t		scanStartMs = 0;
	auto		batchBuildPending = false;

	std::string		bugs;

	auto queuePattern = [ & ] ( const juce::String& patternText, const bool force6581, const bool force8580 )
	{
		auto	patternStr = patternText.replace ( "/", "\\/" );
		if ( patternStr.endsWithChar ( '/' ) )
			patternStr += ".*";

		const regex::Pattern	pattern ( patternStr.toStdString (), true );
		if ( ! pattern.isValid () )
		{
			sendActionMessage ( "error Invalid regex: " + patternText );
			return;
		}

		// New entries splice into the queue in chunks: taking queueLock per
		// subtune would contend with the GUI's per-tick full-queue poll and
		// starve the message thread for a broad pattern's whole expansion.
		// Jobs may start on entries still in a chunk, the pointers are stable
		std::vector<std::unique_ptr<QueueEntry>>	chunk;

		auto flushChunk = [ & ]
		{
			if ( chunk.empty () )
				return;

			const juce::CriticalSection::ScopedLockType	lock ( queueLock );

			// A forced re-add supersedes any existing entry for the same tune:
			// interrupt its render and take it out of the visible queue
			if ( force6581 || force8580 )
			{
				std::unordered_set<std::string>	keys;
				for ( const auto& entry : chunk )
					keys.insert ( entry->name + "#" + std::to_string ( entry->tuneNo ) );

				for ( auto it = queue.begin (); it != queue.end (); )
				{
					if ( keys.contains ( ( *it )->name + "#" + std::to_string ( ( *it )->tuneNo ) ) )
					{
						( *it )->abort = true;
						removedQueue.push_back ( std::move ( *it ) );
						it = queue.erase ( it );
					}
					else
						++it;
				}
			}

			for ( auto& entry : chunk )
				queue.push_back ( std::move ( entry ) );

			chunk.clear ();
		};

		for ( auto& [ name, ent ] : db.db )
		{
			if ( threadShouldExit () )
				break;

			if ( ! pattern.contains ( name ) )
				continue;

			for ( auto tuneNo = 0; const auto len : ent.lengths )
			{
				// Everything queues; the settings fingerprint and the force scope
				// decide inside the render job whether existing measurements are
				// still valid. It may only skip a subtune whose BOTH loudness
				// values were really measured: those are the sole reliable "was
				// rendered" markers, the other columns pad out-of-order gaps with
				// safe defaults
				auto	storedHash = std::string ();
				{
					const auto	loud = tuneNo < static_cast<int> ( ent.loudness.size () ) ? ent.loudness[ tuneNo ] : -96.0f;
					const auto	mid = tuneNo < static_cast<int> ( ent.midLoudness.size () ) ? ent.midLoudness[ tuneNo ] : -96.0f;
					if ( loud > -96.0f && loud != 0.0f && mid > -96.0f && tuneNo < static_cast<int> ( ent.settings.size () ) )
						storedHash = ent.settings[ tuneNo ];
				}

				// The stored verdict picks the faster filter-less emulation where it
				// can, and an unknown tune starts filter-less until proven otherwise;
				// a wrong guess is caught mid-render and re-rendered with the filter
				const auto	hasVerdict = tuneNo < static_cast<int> ( ent.filterUsed.size () );
				const auto	useFilter = hasVerdict && ent.filterUsed[ tuneNo ];

				auto	newEntry = std::make_unique<QueueEntry> ();
				newEntry->name = name;
				newEntry->tuneNo = tuneNo + 1;
				newEntry->lengthMS = len;

				auto* const	queueEntry = newEntry.get ();
				chunk.push_back ( std::move ( newEntry ) );
				if ( chunk.size () >= 1000 )
					flushChunk ();

				if ( ! scanStartMs )
					scanStartMs = juce::Time::currentTimeMillis ();

				tp.addJob ( [ &, queueEntry, useFilter, hasVerdict, storedHash, force6581, force8580 ]
				{
					if ( abortRequested.load () || queueEntry->abort.load () )
						return;

					queueEntry->state = EntryState::running;

					auto	path = resolveTuneSpec ( juce::String ( queueEntry->name ) );

					auto	renderFilter = useFilter;

					MeasureLoudness::result	res;

					for (;;)
					{
						auto	sid = std::make_unique<MeasureLoudness> ( sharedConfig );

						res = sid->measureTune ( path.toRawUTF8 (), queueEntry->tuneNo, queueEntry->lengthMS, renderFilter, storedHash, force6581, force8580, &queueEntry->renderedMS, &queueEntry->speedSample, &queueEntry->features, &queueEntry->abort );

						if ( ! res.filterMismatch )
							break;

						// Discard the filter-less progress and measure again with the filter
						// engaged; only a stale stored verdict is an anomaly worth a warning,
						// an unknown tune upgrading is the designed path
						if ( hasVerdict )
							Z_WARN ( juce::String ( queueEntry->name ) << " tune " << queueEntry->tuneNo << ": stale no-filter verdict, re-measuring with the filter" );

						renderFilter = true;
						queueEntry->features = 0;
						queueEntry->renderedMS = 0;
					}

					queueEntry->completedAtMs = juce::Time::getMillisecondCounter ();

					// An aborted measurement is incomplete (app shutdown, or superseded by a
					// forced re-add): back to pending, don't record anything
					if ( abortRequested.load () || queueEntry->abort.load () )
					{
						queueEntry->features = 0;
						queueEntry->state = EntryState::pending;
						return;
					}

					// Unchanged settings and valid measurements: nothing to re-render
					if ( res.settingsMatch )
					{
						queueEntry->state = EntryState::done;
						return;
					}

					// Store the error on the entry and append it to the bugs file
					if ( ! res.error.empty () )
					{
						queueEntry->error = res.error;
						queueEntry->state = EntryState::failed;

						const juce::CriticalSection::ScopedLockType	lock ( csPrinter );

						bugs.append ( fmt::format ( "{}, tune {} - {}\n", queueEntry->name, queueEntry->tuneNo, res.error ) );

						fileutils::replaceFile ( scannerDataFile ( "bugs.txt" ), bugs.data (), bugs.size () );

						return;
					}

					queueEntry->state = EntryState::done;

					// A jam mid-render is a valid one-shot measurement, but the rip is
					// broken and worth reporting upstream, so it still gets a bugs entry.
					// Near or past the songlength the crash is the rip's own ending,
					// well before it the tune got cut short, a real bug
					if ( res.jammedAtMs != 0 )
					{
						const juce::CriticalSection::ScopedLockType	lock ( csPrinter );

						const auto	atSongEnd = res.jammedAtMs + 2000 >= queueEntry->lengthMS;

						bugs.append ( fmt::format ( "{}{}, tune {} - HLT after {:.1f}s of {:.1f}s ({}), measured as one-shot\n",
													atSongEnd ? "" : "ERROR: ", queueEntry->name, queueEntry->tuneNo, res.jammedAtMs / 1000.0, queueEntry->lengthMS / 1000.0,
													atSongEnd ? "song end" : "MID-SONG" ) );

						fileutils::replaceFile ( scannerDataFile ( "bugs.txt" ), bugs.data (), bugs.size () );
					}

					// Add entry
					{
						const auto	lock = juce::CriticalSection::ScopedLockType ( csPrinter );

						db.addEntry ( queueEntry->name, queueEntry->tuneNo, res.loudness, res.midLoudness, res.filterUsed, res.digiUsed, res.looped, res.startMs, res.settingsHash, res.writeRates, res.digiHint );
						++finishedCnt;
						++measuredTotal;
						if ( finishedCnt == 10 )
						{
							finishedCnt = 0;
							db.saveLUFS ();
							db.saveFilterUsed ();
							db.saveDigiUsed ();
							db.saveLooped ();
							db.saveStarts ();
							db.saveSettings ();
							db.saveWriteRates ();
							db.saveDigiHints ();
						}
					}
				} );

				++tuneNo;
			}
		}

		// Also on early exit: jobs already reference the chunk's entries, the
		// splice keeps them alive
		flushChunk ();
	};

	//
	// Main loop: queue newly added patterns, flush finished measurements while idle
	//
	while ( ! threadShouldExit () )
	{
		juce::String	patternText;
		auto			patternForce6581 = false;
		auto			patternForce8580 = false;
		{
			const juce::CriticalSection::ScopedLockType	lock ( patternLock );

			if ( ! pendingPatterns.empty () )
			{
				patternText = pendingPatterns.front ().pattern;
				patternForce6581 = pendingPatterns.front ().force6581;
				patternForce8580 = pendingPatterns.front ().force8580;
				pendingPatterns.erase ( pendingPatterns.begin () );
			}
		}

		if ( patternText.isNotEmpty () )
		{
			queuePattern ( patternText, patternForce6581, patternForce8580 );
			batchBuildPending = batch;
			continue;
		}

		// Idle with the pool drained: flush any completed measurements. The
		// run total detects the drain, finishedCnt only tracks what the last
		// periodic flush hasn't saved yet (zero when the run ended on one)
		auto	scanFinished = false;
		{
			const juce::CriticalSection::ScopedLockType	lock ( csPrinter );

			if ( measuredTotal && ! tp.getNumJobs () )
			{
				if ( finishedCnt )
				{
					finishedCnt = 0;
					db.saveLUFS ();
					db.saveFilterUsed ();
					db.saveDigiUsed ();
					db.saveLooped ();
					db.saveStarts ();
					db.saveSettings ();
					db.saveWriteRates ();
					db.saveDigiHints ();
				}

				scanFinished = true;
			}
		}

		// A batch run builds after every drained queue, whether or not a
		// measurement was stale: the caller gets a database matching the collection
		const auto	batchDrained = batchBuildPending && ! tp.getNumJobs ();

		// A drained queue just flushed fresh measurement files, so the database
		// rebuilds right away; a manual request waits here too, so it can never
		// read files a running scan is still updating
		if ( ( scanFinished || batchDrained || buildRequested.load () ) && ! tp.getNumJobs () )
		{
			buildRequested = false;
			batchBuildPending = false;

			dbProgress = 0.0f;
			const auto	built = buildDatabase ( &dbProgress ) == 0;
			dbProgress = -1.0f;

			sendActionMessage ( built ? "dbbuilt ok" : "dbbuilt failed" );
		}

		if ( scanFinished )
		{
			sendCompletionMail ( measuredTotal, juce::Time::currentTimeMillis () - scanStartMs );
			measuredTotal = 0;
			scanStartMs = 0;
		}

		wait ( 200 );
	}

	//
	// Shutdown: abort in-flight jobs, then flush what completed
	//
	abortRequested = true;
	{
		const juce::CriticalSection::ScopedLockType	lock ( queueLock );

		for ( auto& entry : queue )
			entry->abort = true;
	}
	tp.removeAllJobs ( true, 10000 );

	{
		const juce::CriticalSection::ScopedLockType	lock ( csPrinter );

		if ( finishedCnt )
		{
			db.saveLUFS ();
			db.saveFilterUsed ();
			db.saveDigiUsed ();
			db.saveLooped ();
			db.saveStarts ();
			db.saveSettings ();
			db.saveWriteRates ();
			db.saveDigiHints ();
		}
	}
}
//-----------------------------------------------------------------------------
