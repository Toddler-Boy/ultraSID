#include "HardwareOutput.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>

#include "USBSID/USBSID_Manager.h"

//-----------------------------------------------------------------------------

namespace
{
	constexpr int64_t	kMaxDelta = 65535;			// Cycles one write can wait, the wire format is 16 bit
	constexpr int		kRingMargin = 32;			// Bytes kept free in a board ring
	constexpr int		kBytesPerWrite = 4;

	// Clock rates the boards know, index 0 is PAL
	constexpr long		kBoardClocks[] = { 985248, 1022727, 1023440 };

	long boardClockFor ( const double cpuHz )
	{
		auto	best = kBoardClocks[ 0 ];

		for ( const auto clk : kBoardClocks )
			if ( std::abs ( double ( clk ) - cpuHz ) < std::abs ( double ( best ) - cpuHz ) )
				best = clk;

		return best;
	}

	// Register order for a snapshot: per voice frequency, pulse width, envelope, then control
	// (gate) last, followed by the filter and volume registers
	constexpr uint8_t	kSnapshotOrder[] =
	{
		0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x04,
		0x07, 0x08, 0x09, 0x0a, 0x0c, 0x0d, 0x0b,
		0x0e, 0x0f, 0x10, 0x11, 0x13, 0x14, 0x12,
		0x15, 0x16, 0x17, 0x18
	};

	class ManagerBackend final : public HardwareBackend
	{
	public:
		bool open ( const std::vector<std::string>& serials )
		{
			return manager.OpenAll ( serials, true, true ) && manager.TotalSIDs () > 0;
		}

		~ManagerBackend () override
		{
			manager.CloseAll ();
		}

		int numBoards () const override	{	return manager.BoardCount ();	}
		int numSids () const override	{	return manager.TotalSIDs ();	}
		int boardOf ( const int sid ) const override	{	return manager.LogicalMap ()[ size_t ( sid ) ].board_index;	}
		int slotOf ( const int sid ) const override		{	return manager.LogicalMap ()[ size_t ( sid ) ].local_slot;	}
		bool isSid ( const int sid ) const override		{	return manager.LogicalMap ()[ size_t ( sid ) ].sid_type != 4;	}	// 4 is an FM OPL chip
		int ringFreeBytes ( const int sid ) override	{	return manager.RingFreeBytes ( sid );						}

		void write ( const int sid, const uint8_t boardReg, const uint8_t val, const uint16_t cycles ) override
		{
			manager.WriteRingCycled ( sid, boardReg, val, cycles );
		}

		void writeBatch ( const int sid, const uint8_t* items, const int count ) override
		{
			manager.WriteRingCycledN ( sid, items, count );
		}

		void flush () override				{	manager.FlushAll ();							}
		void flushBoard ( const int sid ) override	{	manager.FlushBoard ( sid );				}
		void resetRings () override			{	manager.ResetRingBufferAll ();					}
		void mute ( const bool muted ) override	{	manager.SetMutedAll ( muted );				}
		void resetRegisters () override		{	manager.ResetAllRegistersAll ();				}
		void setClock ( const long hz ) override	{	manager.SetClockRateAll ( hz, true );		}

	private:
		USBSID_Manager	manager;
	};
}
//-----------------------------------------------------------------------------

HardwareOutput::HardwareOutput ()
	: ring ( kRingSize )
{
	std::fill_n ( sidOfChip, kMaxChips, -1 );
	std::fill_n ( fillSid, kMaxBoards, -1 );
	std::fill_n ( fillChip, kMaxBoards, -1 );
	std::fill_n ( lastCycle, kMaxBoards, int64_t ( 0 ) );
	std::fill_n ( volume, kMaxChips, uint8_t ( 0 ) );
}
//-----------------------------------------------------------------------------

HardwareOutput::~HardwareOutput ()
{
	close ();
}
//-----------------------------------------------------------------------------

std::vector<USBSID_NS::USBSID_DeviceInfo> HardwareOutput::enumerate ()
{
	return USBSID_Manager::Enumerate ();
}
//-----------------------------------------------------------------------------

bool HardwareOutput::open ( const std::vector<std::string>& serials )
{
	auto	mb = std::make_unique<ManagerBackend> ();

	if ( ! mb->open ( serials ) )
		return false;

	return open ( std::move ( mb ) );
}
//-----------------------------------------------------------------------------

bool HardwareOutput::open ( std::unique_ptr<HardwareBackend> newBackend )
{
	close ();

	if ( ! newBackend || newBackend->numSids () <= 0 )
		return false;

	{
		const std::lock_guard	lk ( ctl );

		backend = std::move ( newBackend );

		usable.clear ();

		for ( auto s = 0; s < backend->numSids (); ++s )
			if ( backend->isSid ( s ) )
				usable.push_back ( s );

		numBoards = std::min ( backend->numBoards (), kMaxBoards );

		for ( auto b = 0; b < numBoards; ++b )
		{
			boards[ b ] = std::make_unique<BoardQueue> ();
			boards[ b ]->ring.resize ( kBoardRingSize );

			for ( auto s = 0; s < backend->numSids (); ++s )
				if ( backend->boardOf ( s ) == b )
				{
					boards[ b ]->sid = s;
					break;
				}
		}

		statBoards = backend->numBoards ();
		statSids = int ( usable.size () );
		statChips = statUnmapped = 0;
		released = dropped = stalls = 0;
		lagMicros = 0;
		ringHead = ringTail = 0;
		armed = paused = detached = false;
		quit = false;
		opened = true;
	}

	for ( auto b = 0; b < numBoards; ++b )
		if ( boards[ b ]->sid >= 0 )
			boards[ b ]->worker = std::thread ( [ this, b ] { boardLoop ( *boards[ b ] ); } );

	writer = std::thread ( [ this ] { writerLoop (); } );

	return true;
}
//-----------------------------------------------------------------------------

void HardwareOutput::close ()
{
	if ( writer.joinable () )
	{
		{
			const std::lock_guard	lk ( ctl );

			quit = true;
		}

		wake.notify_all ();
		writer.join ();
	}

	quit = true;

	for ( auto b = 0; b < numBoards; ++b )
		if ( boards[ b ] )
		{
			boards[ b ]->cv.notify_all ();

			if ( boards[ b ]->worker.joinable () )
				boards[ b ]->worker.join ();
		}

	const std::lock_guard	lk ( ctl );

	if ( backend )
	{
		armed = false;
		drain ();
		drainBoards ();
		silence ();
		backend.reset ();
	}

	for ( auto& q : boards )
		q.reset ();

	numBoards = 0;
	usable.clear ();

	opened = false;
	statBoards = statSids = statChips = statUnmapped = 0;
}
//-----------------------------------------------------------------------------

void HardwareOutput::setPlayheadSource ( std::function<int64_t ()> source )
{
	const std::lock_guard	lk ( ctl );

	playhead = std::move ( source );
}
//-----------------------------------------------------------------------------

bool HardwareOutput::arm ( const uint8_t ( *regs )[ 32 ], const int chips, const int64_t startCycle, const double cpuHz )
{
	if ( ! opened || cpuHz <= 0.0 )
		return false;

	const std::lock_guard	lk ( ctl );

	if ( ! backend )
		return false;

	armed = false;
	drain ();

	// The board workers stand still while the boards are brought to the snapshot
	std::vector<std::unique_lock<std::mutex>>	held;

	for ( auto b = 0; b < numBoards; ++b )
		held.emplace_back ( boards[ b ]->wm );

	drainBoards ();

	cycle0 = startCycle;
	samplesPerCycle = double ( kSampleRate ) / cpuHz;
	anchorPos = 0;
	numChips = std::min ( chips, kMaxChips );
	producedCycle.store ( startCycle, std::memory_order_release );

	std::fill_n ( fillSid, kMaxBoards, -1 );
	std::fill_n ( fillChip, kMaxBoards, -1 );
	std::fill_n ( sidOfChip, kMaxChips, -1 );

	auto	unmapped = 0;

	// Tune chips take the SID slots in order, skipping slots that hold something else
	for ( auto c = 0; c < numChips; ++c )
	{
		const auto	sid = c < int ( usable.size () ) ? usable[ size_t ( c ) ] : -1;
		const auto	board = sid >= 0 ? backend->boardOf ( sid ) : -1;

		if ( board < 0 || board >= numBoards )
		{
			++unmapped;
			continue;
		}

		sidOfChip[ c ] = sid;
		volume[ c ] = regs[ c ][ 0x18 ];

		if ( fillSid[ board ] < 0 )
		{
			fillSid[ board ] = sid;
			fillChip[ board ] = c;
		}
	}

	statChips = numChips;
	statUnmapped = unmapped + std::max ( 0, chips - kMaxChips );

	backend->mute ( true );
	backend->resetRings ();
	backend->resetRegisters ();
	backend->setClock ( boardClockFor ( cpuHz ) );

	// The snapshot restores the state the emulation is in at startCycle
	int64_t	snapshotCycles[ kMaxBoards ] = {};

	for ( auto c = 0; c < numChips; ++c )
	{
		const auto	sid = sidOfChip[ c ];

		if ( sid < 0 )
			continue;

		const auto	board = backend->boardOf ( sid );
		const auto	slotBase = uint8_t ( backend->slotOf ( sid ) * 0x20 );

		for ( const auto reg : kSnapshotOrder )
		{
			backend->write ( sid, uint8_t ( slotBase + reg ), regs[ c ][ reg ], uint16_t ( kSnapshotDelay ) );
			snapshotCycles[ board ] += kSnapshotDelay;
		}
	}

	backend->flush ();

	for ( auto b = 0; b < kMaxBoards; ++b )
		lastCycle[ b ] = startCycle + snapshotCycles[ b ];

	originStarted = false;
	unmutePending = true;
	released = dropped = stalls = 0;
	lagMicros = 0;
	paused = false;
	detached = false;
	armed = true;

	return true;
}
//-----------------------------------------------------------------------------

void HardwareOutput::endTune ()
{
	const std::lock_guard	lk ( ctl );

	const auto	wasActive = armed.exchange ( false );

	drain ();

	if ( wasActive && backend )
	{
		std::vector<std::unique_lock<std::mutex>>	held;

		for ( auto b = 0; b < numBoards; ++b )
			held.emplace_back ( boards[ b ]->wm );

		drainBoards ();
		silence ();
	}

	paused = detached = false;
	statChips = statUnmapped = 0;
}
//-----------------------------------------------------------------------------

void HardwareOutput::setPaused ( const bool pause )
{
	const std::lock_guard	lk ( ctl );

	if ( paused.exchange ( pause ) == pause || ! armed || detached || ! backend )
		return;

	if ( pause )
	{
		backend->mute ( true );
		unmutePending = false;
	}
	else
	{
		reanchor ();
	}
}
//-----------------------------------------------------------------------------

void HardwareOutput::detach ()
{
	const std::lock_guard	lk ( ctl );

	if ( ! armed || detached.exchange ( true ) || ! backend )
		return;

	backend->mute ( true );
	unmutePending = false;
	drain ();

	std::vector<std::unique_lock<std::mutex>>	held;

	for ( auto b = 0; b < numBoards; ++b )
		held.emplace_back ( boards[ b ]->wm );

	drainBoards ();
}
//-----------------------------------------------------------------------------

HardwareOutput::Status HardwareOutput::status () const
{
	Status	s;

	s.open = opened;
	s.armed = armed;
	s.paused = paused;
	s.detached = detached;
	s.boards = statBoards;
	s.sids = statSids;
	s.chips = statChips;
	s.unmappedChips = statUnmapped;
	s.released = released;
	s.dropped = dropped;
	s.stalls = stalls;
	s.lagMs = double ( lagMicros.load () ) / 1000.0;

	return s;
}
//-----------------------------------------------------------------------------

void HardwareOutput::onWrite ( void* user, const uint8_t chip, const uint8_t reg, const uint8_t val, const int64_t cycle ) noexcept
{
	auto&	self = *static_cast<HardwareOutput*> ( user );

	if ( self.armed.load ( std::memory_order_relaxed ) )
		self.push ( { cycle, chip, reg, val } );
}
//-----------------------------------------------------------------------------

void HardwareOutput::push ( const Event& e ) noexcept
{
	const auto	tail = ringTail.load ( std::memory_order_relaxed );

	if ( tail - ringHead.load ( std::memory_order_acquire ) >= kRingSize )
	{
		dropped.fetch_add ( 1, std::memory_order_relaxed );
		return;
	}

	ring[ tail & ( kRingSize - 1 ) ] = e;
	ringTail.store ( tail + 1, std::memory_order_release );
}
//-----------------------------------------------------------------------------

bool HardwareOutput::peek ( Event& e ) const
{
	const auto	head = ringHead.load ( std::memory_order_relaxed );

	if ( head == ringTail.load ( std::memory_order_acquire ) )
		return false;

	e = ring[ head & ( kRingSize - 1 ) ];

	return true;
}
//-----------------------------------------------------------------------------

void HardwareOutput::pop ()
{
	ringHead.store ( ringHead.load ( std::memory_order_relaxed ) + 1, std::memory_order_release );
}
//-----------------------------------------------------------------------------

void HardwareOutput::drain ()
{
	ringHead.store ( ringTail.load ( std::memory_order_acquire ), std::memory_order_release );
}
//-----------------------------------------------------------------------------

void HardwareOutput::drainBoards ()
{
	for ( auto b = 0; b < numBoards; ++b )
		if ( boards[ b ] )
			boards[ b ]->head.store ( boards[ b ]->tail.load ( std::memory_order_acquire ), std::memory_order_release );
}
//-----------------------------------------------------------------------------

bool HardwareOutput::queue ( const int board, const uint8_t reg, const uint8_t val, const uint16_t cycles )
{
	auto&	q = *boards[ board ];
	const auto	tail = q.tail.load ( std::memory_order_relaxed );

	if ( tail - q.head.load ( std::memory_order_acquire ) >= kBoardRingSize )
		return false;

	q.ring[ tail & ( kBoardRingSize - 1 ) ] = { reg, val, cycles };
	q.tail.store ( tail + 1, std::memory_order_release );

	return true;
}
//-----------------------------------------------------------------------------

void HardwareOutput::boardLoop ( BoardQueue& q )
{
	std::unique_lock	lk ( q.wm );

	while ( ! quit )
	{
		auto	sent = false;

		for ( ;; )
		{
			const auto	head = q.head.load ( std::memory_order_relaxed );
			const auto	avail = q.tail.load ( std::memory_order_acquire ) - head;

			if ( avail == 0 )
				break;

			const auto	n = int ( std::min<size_t> ( avail, kBatch ) );

			// The driver ring has no overflow protection, wait for room
			if ( backend->ringFreeBytes ( q.sid ) < n * kBytesPerWrite + kRingMargin )
			{
				stalls.fetch_add ( 1, std::memory_order_relaxed );
				break;
			}

			uint8_t	items[ kBatch * 4 ];

			for ( auto i = 0; i < n; ++i )
			{
				const auto&	o = q.ring[ ( head + size_t ( i ) ) & ( kBoardRingSize - 1 ) ];

				items[ i * 4 ] = o.reg;
				items[ i * 4 + 1 ] = o.val;
				items[ i * 4 + 2 ] = uint8_t ( o.cycles >> 8 );
				items[ i * 4 + 3 ] = uint8_t ( o.cycles & 0xff );
			}

			backend->writeBatch ( q.sid, items, n );
			q.head.store ( head + size_t ( n ), std::memory_order_release );
			sent = true;
		}

		if ( sent )
			backend->flushBoard ( q.sid );

		q.cv.wait_for ( lk, std::chrono::milliseconds ( 1 ) );
	}
}
//-----------------------------------------------------------------------------

void HardwareOutput::silence ()
{
	backend->resetRings ();
	backend->resetRegisters ();
	backend->mute ( false );
	unmutePending = false;
}
//-----------------------------------------------------------------------------

void HardwareOutput::reanchor ()
{
	// Every board has executed what it was sent, so its timeline restarts from the furthest
	// cycle any board reached, and writes wait until the playhead catches up with it
	auto	anchorCycle = cycle0;

	for ( auto b = 0; b < kMaxBoards; ++b )
		if ( fillSid[ b ] >= 0 )
			anchorCycle = std::max ( anchorCycle, lastCycle[ b ] );

	for ( auto b = 0; b < kMaxBoards; ++b )
		lastCycle[ b ] = anchorCycle;

	anchorPos = std::max ( anchorPos, int64_t ( double ( anchorCycle - cycle0 ) * samplesPerCycle ) );
	originStarted = false;
	unmutePending = true;
}
//-----------------------------------------------------------------------------

void HardwareOutput::writerLoop ()
{
	std::unique_lock	lk ( ctl );

	while ( ! quit )
	{
		if ( armed && backend )
			releaseDue ();

		wake.wait_for ( lk, std::chrono::milliseconds ( 1 ) );
	}
}
//-----------------------------------------------------------------------------

void HardwareOutput::releaseDue ()
{
	if ( paused || ! playhead )
		return;

	if ( detached )
	{
		drain ();
		return;
	}

	const auto	ph = playhead ();
	const auto	offset = syncOffsetSamples.load ();

	auto	touched = std::array<bool, kMaxBoards> {};

	auto boardReg = [ this ] ( const int sid, const uint8_t reg )
	{
		return uint8_t ( backend->slotOf ( sid ) * 0x20 + reg );
	};

	if ( ! originStarted )
	{
		if ( ph < anchorPos + offset )
			return;

		originStarted = true;

		if ( unmutePending )
		{
			backend->mute ( false );
			unmutePending = false;

			// The board restores the volume it had when it was muted, which is not always the
			// tune's current one
			for ( auto c = 0; c < numChips; ++c )
			{
				const auto	sid = sidOfChip[ c ];

				if ( sid < 0 )
					continue;

				const auto	board = backend->boardOf ( sid );

				if ( queue ( board, boardReg ( sid, 0x18 ), volume[ c ], uint16_t ( kSnapshotDelay ) ) )
				{
					lastCycle[ board ] += kSnapshotDelay;
					touched[ size_t ( board ) ] = true;
				}
			}
		}
	}

	const auto	horizonPos = ph - offset + leadSamples.load ();
	const auto	horizonCycle = cycle0 + int64_t ( std::floor ( double ( horizonPos ) / samplesPerCycle ) );

	// Read before the ring: every write up to this cycle is already in it
	const auto	produced = producedCycle.load ( std::memory_order_acquire );

	for ( ;; )
	{
		Event	e;
		const auto	have = peek ( e );
		const auto	target = have ? std::min ( e.cycle, horizonCycle ) : std::min ( horizonCycle, produced );

		// A board only waits 65535 cycles per write, and one that runs dry waits from the moment
		// its next write arrives instead of from the previous one. Bridge the idle stretch with
		// harmless volume rewrites so a board's queue always reaches past the playhead
		auto	stalled = false;

		for ( auto b = 0; b < numBoards && ! stalled; ++b )
		{
			const auto	sid = fillSid[ b ];

			if ( sid < 0 )
				continue;

			while ( target - lastCycle[ b ] > kMaxDelta )
			{
				if ( ! queue ( b, boardReg ( sid, 0x18 ), volume[ fillChip[ b ] ], uint16_t ( kMaxDelta ) ) )
				{
					stalled = true;
					break;
				}

				lastCycle[ b ] += kMaxDelta;
				touched[ size_t ( b ) ] = true;
			}
		}

		if ( stalled )
		{
			stalls.fetch_add ( 1, std::memory_order_relaxed );
			break;
		}

		if ( ! have || e.cycle > horizonCycle )
			break;

		const auto	sid = e.chip < numChips ? sidOfChip[ e.chip ] : -1;

		// Registers past $18 are read-only, and some clones use them as a configuration channel
		if ( sid < 0 || e.reg > 0x18 )
		{
			pop ();
			continue;
		}

		const auto	board = backend->boardOf ( sid );
		const auto	delta = std::clamp ( e.cycle - lastCycle[ board ], int64_t ( 0 ), kMaxDelta );

		if ( ! queue ( board, boardReg ( sid, e.reg ), e.val, uint16_t ( delta ) ) )
		{
			stalls.fetch_add ( 1, std::memory_order_relaxed );
			break;
		}

		lastCycle[ board ] = std::max ( lastCycle[ board ], e.cycle );

		if ( e.reg == 0x18 )
			volume[ e.chip ] = e.val;

		released.fetch_add ( 1, std::memory_order_relaxed );
		touched[ size_t ( board ) ] = true;
		pop ();
	}

	for ( auto b = 0; b < numBoards; ++b )
		if ( touched[ size_t ( b ) ] )
			boards[ b ]->cv.notify_one ();

	Event	next;

	if ( peek ( next ) )
	{
		const auto	pos = int64_t ( double ( next.cycle - cycle0 ) * samplesPerCycle );
		const auto	late = std::max ( int64_t ( 0 ), ph - offset - pos );

		lagMicros = late * 1000000 / kSampleRate;
	}
	else
	{
		lagMicros = 0;
	}
}
//-----------------------------------------------------------------------------
