#include <JuceHeader.h>

#include "TuneExporter.h"

#include "libSidplayEZ/src/EZ/SidTuneInfoEZ.h"

#include "ultra-shared/Helpers/PlatformHelper.h"
#include "ultra-shared/Resources/Strings.h"

#include "App/SharedProfiles.h"
#include "Audio/SIDEffects.h"
#include "Audio/SIDPlayer.h"
#include "Helpers/Messages.h"

//-----------------------------------------------------------------------------

// Parallel pipelines out of the box, setNumThreads can override, and the
// cores-2 clamp plus the RAM gate still bound the actual concurrency
static constexpr auto	defaultNumThreads = 4;

// Pool-shrink tuning (see retireSurplusWorkers): pipelines estimated to be
// within this of finishing complete regardless of a lowered thread limit
static constexpr uint32_t	finishThresholdMs = 3000;

// Keep at least this much physical RAM free, an eighth of the machine's
// total, within these bounds. Pausing a render (which parks its full buffers
// + emulator state) and claiming new pipeline work both respect the floor
static constexpr int64_t	memoryFloorMin = 256ll * 1024 * 1024;
static constexpr int64_t	memoryFloorMax = 1024ll * 1024 * 1024;

//-----------------------------------------------------------------------------

static int64_t memoryFloor ()
{
	static const auto	floor = std::clamp ( int64_t ( juce::SystemStats::getMemorySizeInMegabytes () ) * 1024 * 1024 / 8,
											 memoryFloorMin, memoryFloorMax );
	return floor;
}
//-----------------------------------------------------------------------------

// Rough size of a pipeline's render state: ~12 bytes per sample
// (stereo float waveform + digi/register/cycle buffers)
static int64_t estimatedBytes ( const TuneExporter::entry& ent )
{
	return int64_t ( ent.lengthMs ) * 441 / 10 * 12;
}
//-----------------------------------------------------------------------------

// The pause-vs-requeue decision on pool shrink
static bool canPause ( const TuneExporter::entry& ent )
{
	return availableMemoryBytes () - estimatedBytes ( ent ) > memoryFloor ();
}

//-----------------------------------------------------------------------------

// -80 dBFS, the inaudibility bar: ends the trailing-silence trim and keeps
// normalization from boosting a near-silent render's noise floor
static constexpr auto	silenceThreshold = 1.0e-4f;

//-----------------------------------------------------------------------------

// Imprecise songlength entries can leave long digital silence at the tune
// end; trim it to a short pad so exports end where the audio does
static void trimTrailingSilence ( juce::AudioBuffer<float>& src )
{
	constexpr auto	padSamples = 44100 / 20;	// 50 ms of grace after the last audible sample

	const auto	numChannels = src.getNumChannels ();
	const auto	numSamples = src.getNumSamples ();

	auto	lastLoud = -1;
	for ( auto ch = 0; ch < numChannels; ++ch )
	{
		const auto	data = src.getReadPointer ( ch );

		for ( auto i = numSamples - 1; i > lastLoud; --i )
			if ( std::abs ( data[ i ] ) >= silenceThreshold )
			{
				lastLoud = i;
				break;
			}
	}

	if ( const auto trimmed = std::min ( numSamples, lastLoud + 1 + padSamples ); trimmed < numSamples )
		src.setSize ( numChannels, trimmed, true );
}
//-----------------------------------------------------------------------------

// The FX pass of the export pipeline, runs on a worker thread. Blocks travel
// the same route as the live path (process then applyGlain); after the tune
// the buffer grows by silence until the chain reports its tails drained, so
// delay and reverb ring out instead of cutting at the tune end
static bool applyExportFX ( juce::AudioBuffer<float>& src, const int fxMode, const SidTuneInfoEZ& info, std::atomic<uint32_t>& progressMs, const std::function<bool ()>& abort )
{
	constexpr auto	sixtyHzLength = 44100 / 60;

	const auto	isStereo = src.getNumChannels () == 2;
	const auto	numSamples = src.getNumSamples ();

	// With FX is always stereo
	src.setSize ( 2, numSamples, true );

	auto	fx = std::make_unique<SIDEffects> ();

	fx->setFXMode ( fxMode );
	fx->setStereo ( isStereo );
	fx->setChipModel ( ! info.model.empty () && info.model[ 0 ] == "6581", info.clock == "NTSC", info.stereoWidth, info.bassAdjust );
	fx->clearBuffers ();

	// A fresh chain starts drained, arm it (this also opens the noise gates)
	fx->setPlaying ( true );

	float*	inOut[ 2 ] = { src.getWritePointer ( 0 ), src.getWritePointer ( 1 ) };

	auto	length = numSamples;
	auto	done = 0;
	while ( length )
	{
		if ( abort () )
			return false;

		const auto	fxLength = std::min ( length, sixtyHzLength );

		fx->process ( inOut, fxLength );
		fx->applyGlain ( inOut, fxLength );

		inOut[ 0 ] += fxLength;
		inOut[ 1 ] += fxLength;

		length -= fxLength;
		done += fxLength;

		progressMs = uint32_t ( done / 44.1f );
	}

	// Feed silence until the tails have rung out, growing the buffer a second
	// at a time (the drain window itself is one second of quiet)
	fx->setPlaying ( false );

	auto	used = src.getNumSamples ();

	while ( ! fx->drained () )
	{
		if ( abort () )
			return false;

		if ( used == src.getNumSamples () )
			src.setSize ( 2, used + 44100, true, true );

		float*	tail[ 2 ] = { src.getWritePointer ( 0 ) + used, src.getWritePointer ( 1 ) + used };

		fx->process ( tail, sixtyHzLength );
		fx->applyGlain ( tail, sixtyHzLength );

		used += sixtyHzLength;
	}

	src.setSize ( 2, used, true );

	return true;
}
//-----------------------------------------------------------------------------

// Runs on the writer thread only, never in parallel; the entry's extension
// picks the codec (derived from the format preference at enqueue).
// Fed in one-second slices, so progress and abort stay responsive
static bool writeAudioFile ( const std::string& filename, const juce::AudioBuffer<float>& audio, std::atomic<uint32_t>& savingPercent, const std::function<bool ()>& abort )
{
	const auto	f = juce::File ( filename );
	f.deleteFile ();

	std::unique_ptr<juce::AudioFormat>	format;
	auto	quality = 0;

	if ( f.hasFileExtension ( "wav" ) )
	{
		format = std::make_unique<juce::WavAudioFormat> ();
	}
	else if ( f.hasFileExtension ( "flac" ) )
	{
		format = std::make_unique<juce::FlacAudioFormat> ();
		quality = 5;	// default compression, lossless either way
	}
	else if ( f.hasFileExtension ( "ogg" ) )
	{
		format = std::make_unique<juce::OggVorbisAudioFormat> ();
		quality = 7;	// measured transparent on SID renders, 2/3 the q9 size
	}
	else
	{
		Z_ERR ( "No writer for " << filename );
		return false;
	}

	auto	stream = std::unique_ptr<juce::OutputStream> ( f.createOutputStream () );
	if ( stream == nullptr )
		return false;

	const auto	writer = format->createWriterFor ( stream,
		juce::AudioFormatWriterOptions {}
			.withSampleRate ( 44100.0 )
			.withNumChannels ( audio.getNumChannels () )
			.withBitsPerSample ( 16 )
			.withQualityOptionIndex ( quality ) );

	if ( writer == nullptr )
		return false;

	constexpr auto	chunkFrames = 44100;
	const auto		totalFrames = audio.getNumSamples ();

	for ( auto start = 0; start < totalFrames; start += chunkFrames )
	{
		if ( abort () )
			return false;

		const auto	frames = std::min ( chunkFrames, totalFrames - start );

		if ( ! writer->writeFromAudioSampleBuffer ( audio, start, frames ) )
			return false;

		savingPercent = uint32_t ( int64_t ( start + frames ) * 100 / totalFrames );
	}

	return true;
}
//-----------------------------------------------------------------------------

TuneExporter::TuneExporter ()
{
	setNumThreads ( defaultNumThreads );
}
//-----------------------------------------------------------------------------

TuneExporter::~TuneExporter ()
{
	stopThreads ();
}
//-----------------------------------------------------------------------------

void TuneExporter::stopThreads ()
{
	// Wind all threads down in parallel before the queues are destroyed;
	// unfinished entries are dropped
	for ( auto& w : workers )
		w->signalThreadShouldExit ();
	writer.signalThreadShouldExit ();

	for ( auto& w : workers )
		w->stopThread ( -1 );
	writer.stopThread ( -1 );
}
//-----------------------------------------------------------------------------

void TuneExporter::setNumThreads ( int count )
{
	// Never more threads than CPU cores
	const auto	maxThreads = std::max ( 1, juce::SystemStats::getNumCpus () - 2 );
	count = std::clamp ( count, 1, maxThreads );

	const juce::ScopedLock	sl ( lock );

	numThreads = count;

	retireSurplusWorkers ();
	startWorkers ();
}
//-----------------------------------------------------------------------------

void TuneExporter::addEntry ( const entry& queueEntry )
{
	{
		const juce::ScopedLock	sl ( lock );

		auto&	ent = *queue.emplace_back ( std::make_unique<entry> () );

		ent.tuneFilename = queueEntry.tuneFilename;
		ent.subtune = queueEntry.subtune;
		ent.lengthMs = queueEntry.lengthMs;
		ent.fadeOutMs = queueEntry.fadeOutMs;
		ent.startMs = queueEntry.startMs;
		ent.ebuGain = queueEntry.ebuGain;
		ent.fxMode = queueEntry.fxMode;
		ent.useFilter = queueEntry.useFilter;
		ent.normalize = queueEntry.normalize;
		ent.exportFilename = queueEntry.exportFilename;
		ent.date = queueEntry.date ? queueEntry.date : juce::Time::currentTimeMillis ();
		ent.status = queueEntry.status.load ();

		ent.index = int ( queue.size () ) - 1;

		// Reloaded finished entries are records, not work
		if ( ent.status == NEW )
			startWorkers ();
	}

	msg::UpdateExportBadgeUser {}.send ();
}
//-----------------------------------------------------------------------------

void TuneExporter::removeEntry ( const int index )
{
	{
		const juce::ScopedLock	sl ( lock );

		if ( index < 0 || index >= int ( queue.size () ) )
			return;

		auto&	ent = *queue[ index ];
		if ( ent.status == COMPLETE )
			return;

		// In-flight entries poll the flag and mark themselves CANCELED
		ent.cancel = true;

		if ( ent.status == NEW || ent.status == PAUSED )
		{
			ent.pausedPlayer.reset ();	// Free the parked render state right away
			ent.status = CANCELED;
		}
	}

	msg::UpdateExportBadgeUser {}.send ();
}
//-----------------------------------------------------------------------------

void TuneExporter::reAddEntry ( const int index )
{
	{
		const juce::ScopedLock	sl ( lock );

		if ( index < 0 || index >= int ( queue.size () ) )
			return;

		auto&	ent = *queue[ index ];
		if ( ent.status != CANCELED )
			return;

		ent.cancel = false;
		ent.renderProgressMs = 0;
		ent.fxProgressMs = 0;
		ent.savingPercent = 0;
		ent.remainingMs = uint32_t ( -1 );
		ent.status = NEW;

		startWorkers ();
	}

	msg::UpdateExportBadgeUser {}.send ();
}
//-----------------------------------------------------------------------------

TuneExporter::entry& TuneExporter::getEntry ( const int index )
{
	return *queue[ index ];
}
//-----------------------------------------------------------------------------

bool TuneExporter::eraseEntry ( const int index )
{
	const juce::ScopedLock	sl ( lock );

	if ( index < 0 || index >= int ( queue.size () ) )
		return false;

	const auto	ent = queue[ index ].get ();

	const auto	s = ent->status.load ();
	if ( s != COMPLETE && s != CANCELED && s != ERROR )
		return false;

	// A worker may still be letting go of an entry it just finished; write
	// jobs never reference finished entries (they are SAVING until the
	// writer's final setStatus), so checking the workers suffices
	for ( auto& w : workers )
		if ( w->current.load () == ent )
			return false;

	queue.erase ( queue.begin () + index );

	for ( auto i = 0; auto& e : queue )
		e->index = i++;

	return true;
}
//-----------------------------------------------------------------------------

int8_t TuneExporter::getStatus ( const int index )
{
	const juce::ScopedLock	sl ( lock );

	if ( index < 0 || index >= int ( queue.size () ) )
		return ERROR;

	return queue[ index ]->status;
}
//-----------------------------------------------------------------------------

// Lower-case labels only, the visible text lives in the strings yml
static constexpr const char*	statusLabels[] = { "new", "rendering", "applying_fx", "saving", "complete", "canceled", "paused", "error" };

const juce::String& TuneExporter::getStatusString ( const int index )
{
	const juce::ScopedLock	sl ( lock );

	auto	curStatus = int ( ERROR );
	if ( index >= 0 && index <  int ( queue.size () ) )
		curStatus = queue[ index ]->status;

	const juce::SharedResourcePointer<Strings>	strings;

	return strings->get ( juce::String ( "export/status/" ) + statusLabels[ curStatus ] );
}
//-----------------------------------------------------------------------------

const char* TuneExporter::statusLabel ( const int8_t status )
{
	return statusLabels[ status ];
}
//-----------------------------------------------------------------------------

int8_t TuneExporter::statusFromLabel ( const juce::String& label )
{
	for ( auto i = 0; i < int ( std::size ( statusLabels ) ); ++i )
		if ( label == statusLabels[ i ] )
			return int8_t ( i );

	return CANCELED;
}
//-----------------------------------------------------------------------------

std::vector<int> TuneExporter::findEntries ( const std::vector<int8_t>& stati ) const
{
	const juce::ScopedLock	sl ( lock );

	std::vector<int>	res;

	for ( auto i = 0; const auto& ent : queue )
	{
		if ( std::ranges::find ( stati, ent->status.load () ) != stati.end () )
			res.emplace_back ( i );

		++i;
	}

	return res;
}
//-----------------------------------------------------------------------------

int TuneExporter::getNumWorkEntries () const
{
	const juce::ScopedLock	sl ( lock );

	return static_cast<int>( std::ranges::count_if ( queue, [] ( const auto& ent )
													 {
														 const auto	s = ent->status.load ();
														 return s != COMPLETE && s != CANCELED && s != ERROR;
													 } ) );
}
//-----------------------------------------------------------------------------

int TuneExporter::getNumErrorEntries () const
{
	const juce::ScopedLock	sl ( lock );

	return static_cast<int>( std::ranges::count_if ( queue, [] ( const auto& ent ) { return ent->status == ERROR; } ) );
}
//-----------------------------------------------------------------------------

// Called with the lock held
int TuneExporter::activeWorkers () const
{
	auto	count = 0;
	for ( auto& w : workers )
		if ( ! w->retire && w->isThreadRunning () )
			++count;

	return count;
}
//-----------------------------------------------------------------------------

// Called with the lock held. A reduced limit is respected right away: excess
// workers are told to abort mid-entry instead of finishing first. Exceptions
// that finish regardless (the worker then drains itself at its loop top):
// pipelines within finishThresholdMs of done, and the fast FX stage. Aborted
// renders pause (state kept, resumed later) when RAM allows, else requeue,
// the youngest first, so the least work is lost.
void TuneExporter::retireSurplusWorkers ()
{
	auto	excess = activeWorkers () - numThreads;
	if ( excess <= 0 )
		return;

	std::vector<std::pair<Worker*, uint32_t>>	candidates;		// worker + render progress

	for ( auto& w : workers )
	{
		if ( w->retire || ! w->isThreadRunning () )
			continue;

		const auto	ent = w->current.load ();

		// Idle workers cost nothing to retire
		if ( ! ent )
		{
			w->retire = true;
			w->notify ();

			if ( --excess == 0 )
				return;

			continue;
		}

		// Nearly done or in the fast FX stage, let it finish
		if ( ent->status == APPLYING_FX || ent->remainingMs <= finishThresholdMs )
			continue;

		candidates.emplace_back ( w.get (), ent->renderProgressMs.load () );
	}

	// Youngest renders first, if RAM forces a requeue, the least work is lost
	std::ranges::sort ( candidates, {}, &std::pair<Worker*, uint32_t>::second );

	for ( auto& [ w, progress ] : candidates )
	{
		w->retire = true;
		w->notify ();

		if ( --excess == 0 )
			return;
	}
}
//-----------------------------------------------------------------------------

// Called with the lock held, revives/creates workers up to numThreads
void TuneExporter::startWorkers ()
{
	auto	active = activeWorkers ();

	// Un-retiring running workers must not wait for claimable work: a worker
	// still aborting its render holds an entry that is RENDERING right now
	// but about to be parked, skipping it here would strand the entry (and
	// the shrunken pool) until the next addEntry
	for ( auto& w : workers )
	{
		if ( active >= numThreads )
			break;

		if ( w->retire && w->isThreadRunning () )
		{
			w->retire = false;
			++active;
		}
	}

	// No claimable or pending work, threads spawn with the first addEntry
	const auto	hasWork = ! writeQueue.empty ()
					   || std::ranges::any_of ( queue, [] ( const auto& ent )
							{
								const auto	s = ent->status.load ();
								return s == NEW || s == PAUSED;
							} );
	if ( ! hasWork )
		return;

	for ( auto& w : workers )
	{
		if ( active >= numThreads )
			break;

		if ( ! w->isThreadRunning () )
		{
			w->retire = false;
			w->startThread ( juce::Thread::Priority::low );
			++active;
		}
	}

	while ( active < numThreads )
	{
		auto&	w = workers.emplace_back ( std::make_unique<Worker> ( *this ) );
		w->startThread ( juce::Thread::Priority::low );
		++active;
	}

	writer.startThread ( juce::Thread::Priority::low );

	wakeWorkers ();
}
//-----------------------------------------------------------------------------

void TuneExporter::wakeWorkers ()
{
	const juce::ScopedLock	sl ( lock );

	for ( auto& w : workers )
		w->notify ();
}
//-----------------------------------------------------------------------------

// Called with the lock held, claims the next queued entry for this worker
TuneExporter::entry* TuneExporter::claimNextEntry ()
{
	// Paused renders first: they are furthest along, their memory is already
	// parked, and finishing them releases it, so they bypass the RAM gate
	for ( auto& ent : queue )
		if ( ent->status == PAUSED )
		{
			ent->status = RENDERING;
			return ent.get ();
		}

	auto	busyPipelines = 0;
	for ( auto& w : workers )
		if ( w->current.load () )
			++busyPipelines;

	for ( auto& ent : queue )
		if ( ent->status == NEW )
		{
			// RAM gate: don't start a render that would push free memory below
			// the floor, unless nothing else runs, so exports always progress
			if ( busyPipelines && availableMemoryBytes () - estimatedBytes ( *ent ) < memoryFloor () )
				return nullptr;

			ent->remainingMs = uint32_t ( -1 );
			ent->status = RENDERING;
			return ent.get ();
		}

	return nullptr;
}
//-----------------------------------------------------------------------------

void TuneExporter::setStatus ( entry& ent, const int8_t newStatus )
{
	ent.status = newStatus;

	// Stage transitions repaint via the UI's progress polling; everything else
	// (terminal states, requeues, pauses) notifies via the message bus, the
	// only notification safe to fire from a worker thread
	if ( newStatus != RENDERING && newStatus != APPLYING_FX && newStatus != SAVING )
	{
		msg::ExportEntryStatusUpdate { ent.index.load () }.send ();

		// CANCELED only ever arrives on the user's behalf
		if ( newStatus == CANCELED )
			msg::UpdateExportBadgeUser {}.send ();
		else
			msg::UpdateExportBadge {}.send ();
	}
}
//-----------------------------------------------------------------------------

void TuneExporter::workerLoop ( Worker& worker )
{
	while ( ! worker.threadShouldExit () )
	{
		entry*	ent = nullptr;

		{
			const juce::ScopedLock	sl ( lock );

			// Marked surplus by setNumThreads, retire (a later grow revives it)
			if ( worker.retire )
				return;

			// Still over the limit (an exempted pipeline just finished), drain
			if ( activeWorkers () > numThreads )
			{
				worker.retire = true;
				return;
			}

			ent = claimNextEntry ();

			worker.current = ent;
		}

		if ( ! ent )
		{
			// Timed, not infinite: RAM freeing up (the claim gate) has no wake event
			worker.wait ( 1000 );
			continue;
		}

		process ( *ent, worker );

		worker.current = nullptr;

		// This pipeline's memory is free again, a RAM-gated worker may claim now
		wakeWorkers ();
	}
}
//-----------------------------------------------------------------------------

// Hand the entry back to the pool, progress is lost, it restarts from scratch
void TuneExporter::requeueEntry ( entry& ent )
{
	ent.renderProgressMs = 0;
	ent.fxProgressMs = 0;
	ent.remainingMs = uint32_t ( -1 );
	setStatus ( ent, NEW );
	wakeWorkers ();
}
//-----------------------------------------------------------------------------

// The per-entry pipeline: render and FX run here, in parallel across all
// workers; the finished buffer is handed to the writer
void TuneExporter::process ( entry& ent, Worker& worker )
{
	auto abort = [ & ] { return worker.threadShouldExit () || ent.cancel.load () || worker.retire.load (); };

	// Resume a paused render, or start fresh
	std::unique_ptr<SIDPlayer>	player;
	{
		const juce::ScopedLock	sl ( lock );

		player = std::move ( ent.pausedPlayer );
	}

	if ( ! player )
	{
		player = std::make_unique<SIDPlayer> ();

		// Shared config (profiles, overrides, sidid signatures) and ROMs
		{
			const juce::SharedResourcePointer<SharedProfiles>	profiles;

			const auto	[ kernal, basic, character ] = profiles->getRoms ();
			player->setRoms ( kernal, basic, character );

			player->setSharedConfig ( profiles->getPlayerConfig () );
		}

		if ( ! player->loadTune ( ent.tuneFilename ) || ! player->init ( ent.subtune, ent.useFilter ) )
		{
			setStatus ( ent, ERROR );
			return;
		}
	}

	// The abort hook also keeps a live wall-clock estimate of the remaining
	// render time, retireSurplusWorkers lets nearly-done renders finish
	const auto	startTicks = juce::Time::getHighResolutionTicks ();
	const auto	startProgressMs = double ( ent.renderProgressMs.load () );

	auto	retireAborted = false;

	const auto	rendered = player->renderBlocking ( uint32_t ( ent.lengthMs ), uint32_t ( ent.fadeOutMs ), ent.ebuGain, uint32_t ( ent.startMs ), [ & ]
	{
		const auto	progressMs = player->getRenderProgressMS ();
		ent.renderProgressMs = progressMs;

		const auto	wallMs = juce::Time::highResolutionTicksToSeconds ( juce::Time::getHighResolutionTicks () - startTicks ) * 1000.0;
		if ( wallMs > 250.0 && progressMs > startProgressMs )
		{
			const auto	speed = ( progressMs - startProgressMs ) / wallMs;	// emulated ms per wall ms
			ent.remainingMs = uint32_t ( std::max ( 0.0, ent.lengthMs - double ( progressMs ) ) / speed );
		}

		if ( ! abort () )
			return false;

		// Latch the reason: a concurrent pool grow may clear `retire` again
		// before the checks after the render run
		retireAborted = worker.retire.load ();

		return true;
	} );

	if ( ! rendered )
	{
		if ( worker.threadShouldExit () )
			return;

		if ( ent.cancel )
		{
			setStatus ( ent, CANCELED );
			return;
		}

		if ( retireAborted )
		{
			// Pool shrank mid-render: park the full render state if RAM
			// allows, nothing is lost, the work is just deferred
			if ( canPause ( ent ) )
			{
				{
					const juce::ScopedLock	sl ( lock );

					ent.pausedPlayer = std::move ( player );
				}
				setStatus ( ent, PAUSED );
			}
			else
				requeueEntry ( ent );

			return;
		}

		setStatus ( ent, ERROR );	// The emulation stalled
		return;
	}

	auto		buffer = player->takeWaveform ();
	const auto&	info = player->getFileInfo ();

	if ( const auto gain = player->getMeasuredGain (); ! juce::approximatelyEqual ( gain, 1.0f ) )
		buffer.applyGain ( gain );

	trimTrailingSilence ( buffer );

	if ( ent.fxMode != SIDEffects::FXMode::PURE )
	{
		setStatus ( ent, APPLYING_FX );

		// No `retire` in this abort hook: FX is fast and nearly the end of the
		// pipeline, a retiring worker finishes it and drains at the loop top
		if ( ! applyExportFX ( buffer, ent.fxMode, info, ent.fxProgressMs, [ & ] { return worker.threadShouldExit () || ent.cancel.load (); } ) )
		{
			if ( ! worker.threadShouldExit () )
				setStatus ( ent, CANCELED );
			return;
		}
	}

	// One peak pass serves both jobs: the normalize preference lifts the file
	// to -0.3 dB, otherwise the file is only ever attenuated. OGG gets the
	// same -0.3 dB ceiling either way (its decode overshoots encoded peaks),
	// lossless formats scale down on actual clipping only. Near-silent
	// renders stay put
	constexpr auto	normalizeTarget = 0.96605f;	// -0.3 dBFS

	const auto	ceiling = juce::File ( ent.exportFilename ).hasFileExtension ( "ogg" ) ? normalizeTarget : 1.0f;

	if ( const auto maxSmp = buffer.getMagnitude ( 0, buffer.getNumSamples () ); ent.normalize && maxSmp >= silenceThreshold )
		buffer.applyGain ( normalizeTarget / maxSmp );
	else if ( maxSmp > ceiling )
		buffer.applyGain ( ceiling / maxSmp );

	// Hand over to the writer, only it touches the disk. SAVING must land
	// before the job does (same lock): pushed first, an instant writer result
	// (cancel, failed open) could be overwritten by a late SAVING, sticking
	// the entry there forever
	{
		const juce::ScopedLock	sl ( lock );

		setStatus ( ent, SAVING );
		writeQueue.push_back ( writeJob { &ent, std::move ( buffer ) } );
	}

	writer.notify ();
}
//-----------------------------------------------------------------------------

void TuneExporter::writerLoop ( juce::Thread& thread )
{
	while ( ! thread.threadShouldExit () )
	{
		writeJob	job;

		{
			const juce::ScopedLock	sl ( lock );

			if ( ! writeQueue.empty () )
			{
				job = std::move ( writeQueue.front () );
				writeQueue.pop_front ();
			}
		}

		if ( ! job.ent )
		{
			thread.wait ( -1 );
			continue;
		}

		auto&	ent = *job.ent;

		if ( ent.cancel )
		{
			setStatus ( ent, CANCELED );
			continue;
		}

		const auto	ok = writeAudioFile ( ent.exportFilename, job.audio, ent.savingPercent,
										[ & ] { return thread.threadShouldExit () || ent.cancel.load (); } );

		// Never leave a half-written file behind, also on app quit mid-write
		if ( ! ok )
			juce::File ( ent.exportFilename ).deleteFile ();

		if ( thread.threadShouldExit () )
			return;

		if ( ok )
		{
			Z_INFO ( "Exported " << ent.exportFilename );
		}
		else if ( ! ent.cancel )
		{
			Z_ERR ( "Could not save audio file " << ent.exportFilename );
		}

		setStatus ( ent, ent.cancel ? CANCELED : ( ok ? COMPLETE : ERROR ) );
	}
}
//-----------------------------------------------------------------------------
