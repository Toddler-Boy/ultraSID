#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "Audio/SIDPlayer.h"

//-----------------------------------------------------------------------------

// Offline tune exporter. N worker threads run the whole pipeline per entry
// (render, FX) in parallel, then hand the finished buffer to the single
// writer thread. Only the writer touches the disk, one file at a time:
// parallel multi-megabyte writes would just thrash the drive. Workers never
// wait for the disk, the RAM gate alone bounds how far they render ahead.

class TuneExporter final
{
public:
	TuneExporter ();
	~TuneExporter ();

	enum : int8_t
	{
		NEW,
		RENDERING,
		APPLYING_FX,
		SAVING,
		COMPLETE,
		CANCELED,
		PAUSED,		// Parked by a pool shrink, full render state kept, resumes later

		ERROR,
	};

	struct entry
	{
		std::string	tuneFilename;
		int			subtune = 0;
		int			lengthMs = 0;
		int			fadeOutMs = 0;
		int			startMs = 0;	// silent intro, skipped by pre-rendering
		float		ebuGain = 0.0f;
		int			fxMode = 0;
		bool		useFilter = false;
		bool		normalize = false;	// Peak-normalize the finished file, captured from the preference at enqueue

		std::string	exportFilename;

		// When the entry was queued (juce millis); governs list retention.
		// 0 = stamp with "now" on add, reloaded entries carry their saved date
		int64_t		date = 0;

		// Live pipeline state, written by the worker/writer threads, read by the UI
		std::atomic<int8_t>		status = NEW;
		std::atomic<bool>		cancel = false;
		std::atomic<uint32_t>	renderProgressMs = 0;
		std::atomic<uint32_t>	fxProgressMs = 0;
		std::atomic<uint32_t>	savingPercent = 0;
		std::atomic<uint32_t>	remainingMs = uint32_t ( -1 );	// Wall-clock render estimate (see finishThresholdMs)

		// A render parked by a pool shrink, resumed by the next free worker
		std::unique_ptr<SIDPlayer>	pausedPlayer;

		// Position in the export queue; shifts when finished entries are erased
		std::atomic<int>	index = 0;
	};

	void setNumThreads ( int count );
	void addEntry ( const entry& queueEntry );
	void removeEntry ( const int index );

	// Puts a CANCELED entry back to work in place; the caller refreshes the
	// entry's parameters first, a revived export renders like a fresh one
	void reAddEntry ( const int index );

	// Erase a finished (COMPLETE/CANCELED/ERROR) entry and renumber the queue.
	// Returns false while the entry is still live somewhere, callers keep
	// their row lists in lockstep with the return value
	[[ nodiscard ]] bool eraseEntry ( const int index );

	[[ nodiscard ]] entry& getEntry ( const int index );

	[[ nodiscard ]] std::vector<int> findEntries ( const std::vector<int8_t>& stati ) const;
	[[ nodiscard ]] int getNumWorkEntries () const;
	[[ nodiscard ]] int getNumErrorEntries () const;

	[[ nodiscard ]] int8_t getStatus ( const int index );
	[[ nodiscard ]] const juce::String& getStatusString ( const int index );

	// The statuses' stable save/wire names ("complete", ...); the visible text
	// lives in the strings yml
	[[ nodiscard ]] static const char* statusLabel ( const int8_t status );
	[[ nodiscard ]] static int8_t statusFromLabel ( const juce::String& label );	// unknown = CANCELED

	void lockList () { lock.enter (); }
	void releaseLock () { lock.exit (); }
	[[ nodiscard ]] juce::CriticalSection& getLock () { return lock; }

	// Joins all worker/writer threads; must run before the main window dies,
	// workers message UI objects the window owns
	void stopThreads ();

private:
	// Render/FX pipeline threads, one entry at a time per worker. Shrinking
	// the pool retires workers via the `retire` flag: the new limit is
	// respected immediately, a running pipeline aborts and its entry goes
	// back into the queue, no finishing "just this one"
	class Worker final : public juce::Thread
	{
	public:
		Worker ( TuneExporter& e ) : juce::Thread ( "TuneExporter worker" ), exporter ( e ) {}
		~Worker () override	{	stopThread ( -1 );	}

		std::atomic<bool>	retire = false;
		std::atomic<entry*>	current = nullptr;	// For picking the youngest pipelines on shrink

	private:
		void run () override	{	exporter.workerLoop ( *this );	}

		TuneExporter&	exporter;
	};

	// The one thread that writes to disk
	class Writer final : public juce::Thread
	{
	public:
		Writer ( TuneExporter& e ) : juce::Thread ( "TuneExporter writer" ), exporter ( e ) {}
		~Writer () override	{	stopThread ( -1 );	}

	private:
		void run () override	{	exporter.writerLoop ( *this );	}

		TuneExporter&	exporter;
	};

	struct writeJob
	{
		entry*						ent = nullptr;
		juce::AudioBuffer<float>	audio;
	};

	void workerLoop ( Worker& worker );
	void writerLoop ( juce::Thread& thread );

	void process ( entry& ent, Worker& worker );
	void requeueEntry ( entry& ent );
	[[ nodiscard ]] entry* claimNextEntry ();
	void setStatus ( entry& ent, const int8_t newStatus );

	[[ nodiscard ]] int activeWorkers () const;
	void retireSurplusWorkers ();
	void startWorkers ();
	void wakeWorkers ();

	juce::CriticalSection	lock;
	std::deque<std::unique_ptr<entry>>	queue;	// unique_ptr: threads hold entry pointers, erase never moves them
	int						numThreads = 1;

	std::vector<std::unique_ptr<Worker>>	workers;
	std::deque<writeJob>					writeQueue;
	Writer									writer { *this };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( TuneExporter )
};
//-----------------------------------------------------------------------------
