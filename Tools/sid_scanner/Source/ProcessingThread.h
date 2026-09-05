#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
//
// Action messages are prefixed with their type, followed by a space:
//   "error <text>": fatal start-up errors (per-tune failures live in the queue entries)
//   "hvsc <path>|<version>|<entries>": HVSC details, sent once after loading
//   "dbbuilt ok" / "dbbuilt failed": result of a database build
//
//-----------------------------------------------------------------------------

class ProcessingThread final : public juce::Thread,
							   public juce::ActionBroadcaster
{
public:
	ProcessingThread ();
	~ProcessingThread () override;

	void run () override;

	enum class EntryState
	{
		pending,
		running,
		done,
		failed
	};

	struct QueueEntry
	{
		std::string				name;
		int						tuneNo = 0;
		uint32_t				lengthMS = 0;
		std::string				error;	// written before the state turns failed, safe to read once it has

		// MeasureLoudness::FeatureFlags bits, or-ed in live as detections happen
		std::atomic<uint8_t>	features { 0 };
		std::atomic<uint32_t>	renderedMS { 0 };
		std::atomic<uint64_t>	speedSample { 0 };		// (rendered audio ms << 32 | render wall ms), updated every 10s of audio
		std::atomic<uint32_t>	completedAtMs { 0 };	// millisecond tick when the measurement finished
		std::atomic<EntryState>	state { EntryState::pending };
		std::atomic<bool>		abort { false };	// raised to interrupt this entry's measurement
	};

	// Thread-safe access to the processing queue; entry pointers stay valid for the
	// lifetime of this object: entries superseded by a forced re-add disappear from
	// the queue, but their storage is kept alive (see removedQueue)
	int getNumQueueEntries () const;
	const QueueEntry* getQueueEntry ( int index ) const;

	// Queues all tunes matching the regex pattern; callable from any thread.
	// A forced re-measure ignores existing measurements, scoped by chip model
	// (the resolved tune decides inside the render job): force6581 covers tunes
	// with any 6581, force8580 the pure 8580 tunes
	void addPattern ( const juce::String& pattern, bool force6581 = false, bool force8580 = false );

	// Builds once the queue is drained (immediately when idle); a finished scan
	// also builds automatically, so the db always matches the measurement files
	void requestDatabaseBuild ()			{	buildRequested = true;	notify ();	}

	// Completed fraction of a running database build, negative while none runs
	[[ nodiscard ]] float getDbProgress () const	{	return dbProgress.load ();	}

private:
	struct PendingPattern
	{
		juce::String	pattern;
		bool			force6581 = false;
		bool			force8580 = false;
	};

	std::vector<std::unique_ptr<QueueEntry>>	queue;

	// Entries superseded by a forced re-add: no longer visible, but kept alive so
	// pointers the GUI took from its last queue snapshot can't dangle
	std::vector<std::unique_ptr<QueueEntry>>	removedQueue;

	mutable juce::CriticalSection				queueLock;

	std::vector<PendingPattern>					pendingPatterns;
	juce::CriticalSection						patternLock;

	// Raised on shutdown so in-flight measurements bail out instead of running to completion
	std::atomic<bool>							abortRequested { false };

	std::atomic<bool>							buildRequested { false };
	std::atomic<float>							dbProgress { -1.0f };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ( ProcessingThread )
};
//-----------------------------------------------------------------------------
