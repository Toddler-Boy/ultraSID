#pragma once

//
// Forwards the emulated SID register writes of the playing tune to real USBSID-Pico boards.
//
// The emulation thread pushes every write, with its emulated cycle, into a lock-free ring.
// A writer thread holds each write until the audible playhead approaches its sample position,
// then hands it to the boards with a per-board cycle delta. Nothing here touches JUCE.
//

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "USBSID/USBSID.h"

//-----------------------------------------------------------------------------

/**
* @brief: Board access used by HardwareOutput. Production code wraps USBSID_Manager,
* tests supply a recorder.
*/
class HardwareBackend
{
public:
	virtual ~HardwareBackend () = default;

	[[ nodiscard ]] virtual int numBoards () const = 0;
	[[ nodiscard ]] virtual int numSids () const = 0;					// Logical SIDs over all boards
	[[ nodiscard ]] virtual int boardOf ( int sid ) const = 0;
	[[ nodiscard ]] virtual int slotOf ( int sid ) const = 0;			// Socket slot on its board, 0..3

	// False for a slot that holds no SID, such as an FM OPL chip
	[[ nodiscard ]] virtual bool isSid ( int sid ) const	{	(void)sid;	return true;	}

	// Free bytes in the write ring of the board owning sid, four bytes per cycled write
	[[ nodiscard ]] virtual int ringFreeBytes ( int sid ) = 0;

	// boardReg is the board-local register with the slot folded in ( slot * 0x20 + reg )
	virtual void write ( int sid, uint8_t boardReg, uint8_t val, uint16_t cycles ) = 0;

	// count x ( boardReg, val, cycles high, cycles low ) for the board owning sid
	virtual void writeBatch ( const int sid, const uint8_t* items, const int count )
	{
		for ( auto i = 0; i < count; ++i )
			write ( sid, items[ i * 4 ], items[ i * 4 + 1 ], uint16_t ( items[ i * 4 + 2 ] << 8 | items[ i * 4 + 3 ] ) );
	}

	virtual void flush () = 0;										// Every board
	virtual void flushBoard ( const int sid )	{	(void)sid;	flush ();	}
	virtual void resetRings () = 0;

	// Sets or clears the boards' global muted flag, which keeps volume writes silent while set
	virtual void mute ( bool muted ) = 0;
	virtual void resetRegisters () = 0;
	virtual void setClock ( long hz ) = 0;
};
//-----------------------------------------------------------------------------

class HardwareOutput final
{
public:
	static constexpr int	kMaxChips = 16;
	static constexpr int	kSampleRate = 44100;

	struct Status
	{
		bool		open = false;
		bool		armed = false;
		bool		paused = false;
		bool		detached = false;
		int			boards = 0;
		int			sids = 0;			// Logical SIDs the open boards provide
		int			chips = 0;			// Chips the current tune uses
		int			unmappedChips = 0;	// Tune chips beyond the logical SIDs, not forwarded
		uint64_t	released = 0;		// Writes handed to the boards
		uint64_t	dropped = 0;		// Writes lost to a full event ring
		uint64_t	stalls = 0;			// Times a full board ring held back the writer
		double		lagMs = 0.0;		// How far the oldest pending write is past its due time
	};

	HardwareOutput ();
	~HardwareOutput ();

	HardwareOutput ( const HardwareOutput& ) = delete;
	HardwareOutput& operator= ( const HardwareOutput& ) = delete;

	/**
	* @brief: List attached USBSID-Pico boards without opening them.
	*/
	[[ nodiscard ]] static std::vector<USBSID_NS::USBSID_DeviceInfo> enumerate ();

	/**
	* @brief: Open boards and start the writer thread.
	* @param: serials Boards to open in logical order, empty opens every attached board.
	* @return: False when no board could be opened.
	*/
	bool open ( const std::vector<std::string>& serials );

	/**
	* @brief: Start the writer thread on a supplied backend.
	* @return: False when the backend has no SIDs.
	*/
	bool open ( std::unique_ptr<HardwareBackend> backend );

	/**
	* @brief: Stop the writer thread, silence and release the boards.
	*/
	void close ();

	[[ nodiscard ]] bool isOpen () const	{	return opened.load ();	}
	[[ nodiscard ]] bool isArmed () const	{	return armed.load ();	}

	/**
	* @brief: Set the audible playhead in samples, on the same axis as arm ()'s sample origin.
	* Called from the writer thread, so it must be thread-safe. Set before arm ().
	*/
	void setPlayheadSource ( std::function<int64_t ()> source );

	// How far ahead of the playhead writes are handed to the boards
	void setLeadMs ( const int ms )			{	leadSamples = int64_t ( ms ) * kSampleRate / 1000;	}

	// Shifts the hardware against the audio, negative moves it earlier
	void setSyncOffsetMs ( const int ms )	{	syncOffsetSamples = int64_t ( ms ) * kSampleRate / 1000;	}

	/**
	* @brief: Bring the boards to a tune's state and start forwarding writes.
	* Sets the clock, resets the boards and their rings, writes the register snapshot,
	* and unmutes. Call from the emulation thread before the tune's audible start, then
	* install onWrite () as the emulation's write sink.
	* @param: regs Register snapshot per chip, as the emulation's getSidStatus ().
	* @param: numChips Number of chips in regs.
	* @param: cycle0 Emulated cycle the snapshot belongs to.
	* @param: cpuHz CPU clock of the tune, selects the board clock.
	* @return: False when not open.
	*/
	bool arm ( const uint8_t ( *regs )[ 32 ], int numChips, int64_t cycle0, double cpuHz );

	/**
	* @brief: Report how far the emulation has run. Call from the emulation thread after each
	* chunk, once every write up to that cycle has reached onWrite (). Idle boards are only
	* bridged up to this cycle, as a later write could still fall inside the bridged stretch.
	*/
	void setEmulatedCycle ( const int64_t cycle )	{	producedCycle.store ( cycle, std::memory_order_release );	}

	/**
	* @brief: Stop forwarding, drop pending writes and leave the boards silent and unmuted.
	* Safe to call when not armed.
	*/
	void endTune ();

	/**
	* @brief: Hold or continue forwarding, muting the boards while held.
	*/
	void setPaused ( bool paused );

	/**
	* @brief: Give up sync for the rest of the tune, mutes the boards and drops writes.
	* Used when the emulation moves in a way the boards cannot follow.
	*/
	void detach ();

	[[ nodiscard ]] Status status () const;

	/**
	* @brief: Write sink for the emulation, pass as the sink function with this as user data.
	*/
	static void onWrite ( void* user, uint8_t chip, uint8_t reg, uint8_t val, int64_t cycle ) noexcept;

private:
	struct Event
	{
		int64_t		cycle;
		uint8_t		chip;
		uint8_t		reg;
		uint8_t		val;
	};

	static constexpr size_t		kRingSize = 1u << 16;
	static constexpr uint32_t	kSnapshotDelay = 16;	// Cycles between snapshot writes
	static constexpr int		kMaxBoards = 16;

	// Event ring, single producer (emulation thread), single consumer (whoever holds ctl)
	std::vector<Event>		ring;
	std::atomic<size_t>		ringHead = 0;
	std::atomic<size_t>		ringTail = 0;

	// One writer thread per board, so a board that is slow to accept data never holds
	// back another. The dispatcher (writerLoop) fills the queues, every entry carries
	// its cycle delta already
	struct Out
	{
		uint8_t		reg;
		uint8_t		val;
		uint16_t	cycles;
	};

	struct BoardQueue
	{
		std::vector<Out>		ring;
		std::atomic<size_t>		head = 0;
		std::atomic<size_t>		tail = 0;
		std::mutex				wm;				// Held while the worker sends, control code takes it to quiesce
		std::condition_variable	cv;
		std::thread				worker;
		int						sid = -1;		// Any SID of the board, addresses it
	};

	static constexpr size_t		kBoardRingSize = 1u << 15;
	static constexpr int		kBatch = 32;

	std::unique_ptr<BoardQueue>	boards[ 16 ];
	int							numBoards = 0;
	std::vector<int>			usable;			// Logical SIDs that hold a SID, in order

	std::unique_ptr<HardwareBackend>	backend;
	std::thread				writer;
	std::mutex				ctl;					// Guards backend access and consumer state below
	std::condition_variable	wake;
	std::atomic<bool>		quit = false;

	std::atomic<bool>		opened = false;
	std::atomic<bool>		armed = false;
	std::atomic<bool>		paused = false;
	std::atomic<bool>		detached = false;

	std::function<int64_t ()>	playhead;			// Guarded by ctl
	std::atomic<int64_t>		leadSamples = int64_t ( 100 ) * kSampleRate / 1000;
	std::atomic<int64_t>		syncOffsetSamples = 0;
	std::atomic<int64_t>		producedCycle = 0;

	// Consumer state, guarded by ctl
	int64_t		cycle0 = 0;							// Emulated cycle of sample position 0
	double		samplesPerCycle = 0.0;
	int64_t		anchorPos = 0;						// Sample position where the boards' timeline (re)starts
	bool		originStarted = false;				// Playhead reached the anchor, writes may flow
	bool		unmutePending = false;
	int			numChips = 0;
	int			sidOfChip[ kMaxChips ];				// -1 when not forwarded
	int			fillSid[ kMaxBoards ];				// A forwarded SID per board, -1 for boards without one
	int			fillChip[ kMaxBoards ];
	int64_t		lastCycle[ kMaxBoards ];			// Emulated cycle the board's timeline has reached
	uint8_t		volume[ kMaxChips ];				// Last $18 value per chip, rewritten to bridge idle gaps

	std::atomic<uint64_t>	released = 0;
	std::atomic<uint64_t>	dropped = 0;
	std::atomic<uint64_t>	stalls = 0;
	std::atomic<int64_t>	lagMicros = 0;
	std::atomic<int>		statBoards = 0;
	std::atomic<int>		statSids = 0;
	std::atomic<int>		statChips = 0;
	std::atomic<int>		statUnmapped = 0;

	void writerLoop ();
	void boardLoop ( BoardQueue& q );
	[[ nodiscard ]] bool queue ( int board, uint8_t reg, uint8_t val, uint16_t cycles );	// Needs ctl
	void drainBoards ();								// Needs ctl and every wm
	void releaseDue ();									// Needs ctl
	void drain ();										// Needs ctl
	void silence ();									// Needs ctl
	void reanchor ();									// Needs ctl

	[[ nodiscard ]] bool peek ( Event& e ) const;
	void pop ();
	void push ( const Event& e ) noexcept;
};
//-----------------------------------------------------------------------------
