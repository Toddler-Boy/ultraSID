// HardwareOutput logic test against a recording backend, no boards and no JUCE.
//
//   Tests/hardwareoutput_test.sh
//
// Exit 0: all checks pass. Exit 1: a check failed.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "../Source/Audio/HardwareOutput.h"

namespace
{

int	failures = 0;

#define CHECK( cond )	do { if ( ! ( cond ) ) { std::printf ( "  FAIL line %d: %s\n", __LINE__, #cond ); ++failures; } } while ( 0 )

struct Write
{
	int			sid;
	uint8_t		reg;		// Board-local, slot folded in
	uint8_t		val;
	uint16_t	cycles;
};

// Three SIDs: sid 0 and 1 on board 0 (slots 0, 1), sid 2 on board 1 (slot 0)
class Recorder final : public HardwareBackend
{
public:
	std::mutex				lock;
	std::vector<Write>		writes;
	std::atomic<int>		free { 8000 };
	int						fmSid = -1;		// Slot holding an FM OPL chip instead of a SID

	std::atomic<int>		mutes { 0 }, unmutes { 0 }, resets { 0 }, ringResets { 0 }, flushes { 0 };
	long					clock = 0;

	int numBoards () const override		{	return 2;	}
	int numSids () const override		{	return 3;	}
	int boardOf ( const int sid ) const override	{	return sid == 2 ? 1 : 0;	}
	int slotOf ( const int sid ) const override		{	return sid == 1 ? 1 : 0;	}
	bool isSid ( const int sid ) const override		{	return sid != fmSid;	}
	int ringFreeBytes ( int ) override	{	return free;	}

	void write ( const int sid, const uint8_t reg, const uint8_t val, const uint16_t cycles ) override
	{
		const std::lock_guard	lk ( lock );
		writes.push_back ( { sid, reg, val, cycles } );
	}

	void flush () override			{	++flushes;	}
	void resetRings () override		{	++ringResets;	}
	void mute ( const bool m ) override	{	m ? ++mutes : ++unmutes;	}
	void resetRegisters () override	{	++resets;	}
	void setClock ( const long hz ) override	{	clock = hz;	}

	std::vector<Write> snapshot ()
	{
		const std::lock_guard	lk ( lock );
		return writes;
	}

	void clear ()
	{
		const std::lock_guard	lk ( lock );
		writes.clear ();
	}
};

struct Rig
{
	Recorder*				rec;
	HardwareOutput			hw;
	std::atomic<int64_t>	playhead { -100000 };
	uint8_t					regs[ 3 ][ 32 ] = {};

	Rig ()
	{
		auto	r = std::make_unique<Recorder> ();
		rec = r.get ();
		hw.open ( std::move ( r ) );
		hw.setPlayheadSource ( [ this ] { return playhead.load (); } );
		regs[ 0 ][ 0x18 ] = 0x0f;
		regs[ 1 ][ 0x18 ] = 0x0a;
	}

	void arm ( const int64_t cycle0 = 1000 )	{	CHECK ( hw.arm ( regs, 3, cycle0, 985248.0 ) );	}
	void send ( const uint8_t chip, const uint8_t reg, const uint8_t val, const int64_t cycle )
	{
		HardwareOutput::onWrite ( &hw, chip, reg, val, cycle );
		hw.setEmulatedCycle ( cycle );
	}
};

void settle ()
{
	std::this_thread::sleep_for ( std::chrono::milliseconds ( 60 ) );
}

constexpr int	kSnapshotWrites = 25;	// Per chip

// Idle-gap bridging and the volume restore after an unmute, not part of the emulated stream
bool isPadding ( const Write& w )
{
	return ( w.cycles == 65535 || w.cycles == 16 ) && ( w.reg & 0x1f ) == 0x18;
}

// Batches address a board by one of its SIDs, the socket slot in the register says which chip
int chipOf ( const Write& w )
{
	return w.sid == 2 ? 2 : ( w.reg >> 5 );
}

int boardOfSid ( const int sid )
{
	return sid == 2 ? 1 : 0;
}

// Non-snapshot writes on a sid
std::vector<Write> after ( const std::vector<Write>& all, const size_t skip )
{
	return { all.begin () + ptrdiff_t ( std::min ( skip, all.size () ) ), all.end () };
}

void testTimeline ()
{
	std::printf ( "timeline, gaps, filters\n" );

	Rig	rig;
	rig.arm ();

	// The clock is switched to the tune's rate and the snapshot covers every chip
	CHECK ( rig.rec->clock == 985248 );
	CHECK ( rig.rec->writes.size () == size_t ( 3 * kSnapshotWrites ) );

	// Ordered writes: chip 0 on board 0, chip 2 on board 1, and later a >65535 cycle gap
	rig.send ( 0, 0x00, 0x11, 4100 );
	rig.send ( 2, 0x01, 0x22, 4150 );
	rig.send ( 0, 0x1d, 0x99, 4160 );		// Past $18: not forwarded
	rig.send ( 5, 0x00, 0x55, 4170 );		// Chip the boards do not have: not forwarded
	rig.send ( 1, 0x02, 0x33, 4200 );		// Chip 1 sits in slot 1, board reg $22
	rig.send ( 0, 0x18, 0x07, 4300 );
	rig.send ( 0, 0x03, 0x44, 4300 + 150000 );

	// Before the playhead reaches the origin nothing flows
	settle ();
	CHECK ( rig.rec->snapshot ().size () == size_t ( 3 * kSnapshotWrites ) );

	// At the origin everything inside the lead window is released, the far write is not
	rig.playhead = 0;
	settle ();
	auto	w = after ( rig.rec->snapshot (), 3 * kSnapshotWrites );

	// Per board the order is the emulated one, across boards it is not defined
	std::vector<Write>	board0, board1;

	for ( const auto& x : w )
		( boardOfSid ( x.sid ) == 0 ? board0 : board1 ).push_back ( x );

	std::vector<Write>	events0, events1;

	for ( const auto& x : board0 )
		if ( ! isPadding ( x ) )
			events0.push_back ( x );

	for ( const auto& x : board1 )
		if ( ! isPadding ( x ) )
			events1.push_back ( x );

	CHECK ( events0.size () == 3 );
	CHECK ( events1.size () == 1 );

	if ( events0.size () == 3 && events1.size () == 1 )
	{
		CHECK ( events0[ 0 ].sid == 0 && events0[ 0 ].reg == 0x00 && events0[ 0 ].val == 0x11 );
		CHECK ( chipOf ( events0[ 1 ] ) == 1 && events0[ 1 ].reg == 0x22 && events0[ 1 ].val == 0x33 );
		CHECK ( chipOf ( events0[ 2 ] ) == 0 && events0[ 2 ].reg == 0x18 && events0[ 2 ].val == 0x07 );
		CHECK ( events1[ 0 ].sid == 2 && events1[ 0 ].reg == 0x01 && events1[ 0 ].val == 0x22 );
	}

	// The volume restore after the unmute comes first on every board, with the snapshot volume
	CHECK ( ! board0.empty () && board0[ 0 ].reg == 0x18 && board0[ 0 ].val == 0x0f && board0[ 0 ].cycles == 16 );
	CHECK ( ! board1.empty () && board1[ 0 ].reg == 0x18 && board1[ 0 ].val == 0x00 && board1[ 0 ].cycles == 16 );

	// Board 0 timeline: snapshot ends at cycle0 + 16 * 2 * 25, so the first delta absorbs it
	const auto	snapBoard0 = int64_t ( 2 * kSnapshotWrites * 16 );
	int64_t		t0 = 1000 + snapBoard0;
	auto		expected0 = std::vector<int64_t> { 4100, 4200, 4300 };
	size_t		k = 0;

	for ( const auto& x : board0 )
	{
		t0 += x.cycles;

		if ( ! isPadding ( x ) )
		{
			if ( k < expected0.size () )
				CHECK ( t0 == expected0[ k ] );
			++k;
		}
	}

	CHECK ( k >= 3 );

	// Advance the playhead: the far write comes out, split by fillers, delta sum exact
	rig.rec->clear ();
	rig.playhead = 200000LL * 44100 / 985248;
	settle ();
	auto	far = rig.rec->snapshot ();
	bool	found = false;

	for ( const auto& x : far )
		if ( x.sid == 0 && x.reg == 0x03 && x.val == 0x44 )
			found = true;

	CHECK ( found );

	// Fillers before the event are volume rewrites carrying the last $18 value ($07)
	for ( const auto& x : far )
		if ( x.cycles == 65535 && x.sid == 0 )
			CHECK ( x.reg == 0x18 && x.val == 0x07 );

	CHECK ( rig.hw.status ().released >= 4 );
	CHECK ( rig.hw.status ().dropped == 0 );
	CHECK ( rig.hw.status ().unmappedChips == 0 );
}

void testDeltas ()
{
	std::printf ( "per-board delta sums equal emulated cycle offsets\n" );

	Rig	rig;
	rig.arm ( 5000 );

	int64_t	cycle = 5000 + 3000;

	for ( auto i = 0; i < 2000; ++i )
	{
		cycle += 20 + ( i * 37 ) % 900;
		rig.send ( uint8_t ( i % 3 ), uint8_t ( i % 0x19 ), uint8_t ( i ), cycle );
	}

	rig.playhead = ( cycle - 5000 ) * 44100 / 985248 + 44100;
	settle ();

	// Rebuild each board's timeline and compare to the emulated cycles
	auto	all = after ( rig.rec->snapshot (), 3 * kSnapshotWrites );
	int64_t	t[ 2 ] = { 5000 + 16 * 2 * kSnapshotWrites, 5000 + 16 * kSnapshotWrites };
	size_t	ev = 0;
	int64_t	c2 = 5000 + 3000;
	std::vector<int64_t>	cycles;

	for ( auto i = 0; i < 2000; ++i )
	{
		c2 += 20 + ( i * 37 ) % 900;
		cycles.push_back ( c2 );
	}

	// Chip 0 and 1 sit on board 0, chip 2 on board 1, the boards do not report in a fixed order
	size_t	evBoard[ 2 ] = { 0, 0 };
	std::vector<int64_t>	cyclesOf[ 2 ];
	std::vector<int>		sidsOf[ 2 ];

	for ( size_t i = 0; i < cycles.size (); ++i )
	{
		cyclesOf[ i % 3 == 2 ? 1 : 0 ].push_back ( cycles[ i ] );
		sidsOf[ i % 3 == 2 ? 1 : 0 ].push_back ( int ( i % 3 ) );
	}

	for ( const auto& x : all )
	{
		const auto	b = boardOfSid ( x.sid );

		t[ b ] += x.cycles;

		if ( isPadding ( x ) )
			continue;

		if ( evBoard[ b ] < cyclesOf[ b ].size () )
		{
			CHECK ( t[ b ] == cyclesOf[ b ][ evBoard[ b ] ] );
			CHECK ( chipOf ( x ) == sidsOf[ b ][ evBoard[ b ] ] );
			++evBoard[ b ];
		}
	}

	ev = evBoard[ 0 ] + evBoard[ 1 ];

	CHECK ( ev == 2000 );
}

void testPauseResume ()
{
	std::printf ( "pause and resume\n" );

	Rig	rig;
	rig.arm ();
	rig.playhead = 0;
	settle ();
	rig.rec->clear ();

	rig.hw.setPaused ( true );
	CHECK ( rig.rec->mutes >= 2 );

	rig.send ( 0, 0x00, 0x01, 1000 + 1500 );
	settle ();
	CHECK ( rig.rec->snapshot ().empty () );

	// Resume re-anchors: nothing is sent, and the boards stay muted, until the playhead
	// reaches the end of what the boards already executed (about 2969 samples)
	const int	unmutes = rig.rec->unmutes;
	rig.hw.setPaused ( false );
	settle ();
	CHECK ( rig.rec->snapshot ().empty () );
	CHECK ( rig.rec->unmutes == unmutes );

	rig.playhead = 4000;
	settle ();
	CHECK ( rig.rec->unmutes == unmutes + 1 );

	auto	found = false;

	for ( const auto& x : rig.rec->snapshot () )
		found = found || ( x.reg == 0x00 && x.val == 0x01 );

	CHECK ( found );
}

void testFmOpl ()
{
	std::printf ( "an FM OPL slot is skipped\n" );

	auto	r = std::make_unique<Recorder> ();
	auto*	rec = r.get ();
	rec->fmSid = 1;

	HardwareOutput			hw;
	std::atomic<int64_t>	playhead { 0 };
	uint8_t					regs[ 3 ][ 32 ] = {};

	hw.open ( std::move ( r ) );
	hw.setPlayheadSource ( [ &playhead ] { return playhead.load (); } );

	// Three tune chips, two SID slots: chip 1 lands past the FM slot on sid 2, chip 2 has no slot
	CHECK ( hw.arm ( regs, 3, 1000, 985248.0 ) );
	CHECK ( hw.status ().sids == 2 );
	CHECK ( hw.status ().unmappedChips == 1 );

	HardwareOutput::onWrite ( &hw, 1, 0x05, 0x77, 4000 );
	HardwareOutput::onWrite ( &hw, 2, 0x05, 0x88, 4100 );
	hw.setEmulatedCycle ( 4100 );
	settle ();

	auto	seen = 0;
	auto	slotOne = 0;

	for ( const auto& x : rec->snapshot () )
	{
		if ( x.reg == 0x05 && x.val == 0x77 )
			++seen;

		if ( x.val == 0x88 || ( x.reg >> 5 ) == 1 )
			++slotOne;
	}

	CHECK ( seen == 1 );
	CHECK ( slotOne == 0 );
}

void testBackpressure ()
{
	std::printf ( "full board ring holds writes back, none lost\n" );

	Rig	rig;
	rig.arm ();
	rig.rec->free = 0;
	rig.playhead = 0;

	for ( auto i = 0; i < 100; ++i )
		rig.send ( 0, 0x00, uint8_t ( i ), 1100 + i * 10 );

	settle ();
	CHECK ( rig.hw.status ().stalls > 0 );
	CHECK ( rig.rec->snapshot ().size () == size_t ( 3 * kSnapshotWrites ) );

	rig.rec->free = 8000;
	settle ();

	auto	all = after ( rig.rec->snapshot (), 3 * kSnapshotWrites );
	auto	n = 0;

	for ( const auto& x : all )
		if ( x.sid == 0 && x.reg == 0x00 )
			++n;

	CHECK ( n == 100 );
}

void testOverflow ()
{
	std::printf ( "event ring overflow is counted\n" );

	Rig	rig;
	rig.arm ();
	rig.hw.setPaused ( true );

	for ( auto i = 0; i < 70000; ++i )
		rig.send ( 0, 0x00, 0, 1100 + i );

	CHECK ( rig.hw.status ().dropped == 70000 - ( 1u << 16 ) );
}

void testEndTune ()
{
	std::printf ( "endTune leaves boards silent and unmuted\n" );

	Rig	rig;
	rig.arm ();
	rig.playhead = 0;
	settle ();

	const int	resets = rig.rec->resets;
	rig.hw.endTune ();
	CHECK ( rig.rec->resets == resets + 1 );
	CHECK ( ! rig.hw.status ().armed );

	// Writes after the end are ignored
	rig.send ( 0, 0x00, 0x01, 5000 );
	rig.playhead = 100000;
	settle ();
	CHECK ( rig.rec->snapshot ().size () <= size_t ( 3 * kSnapshotWrites + 4 ) );
}

}

int main ()
{
	testTimeline ();
	testDeltas ();
	testPauseResume ();
	testFmOpl ();
	testBackpressure ();
	testOverflow ();
	testEndTune ();

	std::printf ( failures ? "FAILED (%d)\n" : "ok\n", failures );

	return failures ? 1 : 0;
}
