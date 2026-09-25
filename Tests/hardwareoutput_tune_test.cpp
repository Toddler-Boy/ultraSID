// End to end check without boards: libSidplayEZ write sink -> HardwareOutput -> recording backend.
//
//   Tests/hardwareoutput_tune_test.sh <tune.sid>...
//
// The playhead follows the wall clock while the emulation is held 150 ms ahead of it, the way the
// player does. Checks that every forwarded write arrives once and in order, nothing is dropped, and
// each board's summed cycle deltas match the emulated time (the clock rate of the tune).
//
// Exit 0: all checks pass. Exit 1: a check failed or a tune did not load.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "../Source/Audio/HardwareOutput.h"
#include "../Source/libSidplayEZ/src/player.h"
#include "../Source/libSidplayEZ/src/sidplayfp/SidTune.h"

namespace
{

struct Write
{
	int			sid;
	uint8_t		reg;
	uint8_t		val;
	uint16_t	cycles;
};

// One board holding every SID of the tune, slot = sid
class Recorder final : public HardwareBackend
{
public:
	explicit Recorder ( const int sids ) : sids ( sids ) {}

	std::mutex			lock;
	std::vector<Write>	writes;

	int numBoards () const override					{	return 1;		}
	int numSids () const override					{	return sids;	}
	int boardOf ( int ) const override				{	return 0;		}
	int slotOf ( const int sid ) const override		{	return sid;		}
	int ringFreeBytes ( int ) override				{	return 1 << 20;	}

	void write ( const int sid, const uint8_t reg, const uint8_t val, const uint16_t cycles ) override
	{
		const std::lock_guard	lk ( lock );
		writes.push_back ( { sid, uint8_t ( reg & 0x1f ), val, cycles } );
	}

	void writeBatch ( int, const uint8_t* items, const int count ) override
	{
		for ( auto i = 0; i < count; ++i )
			write ( items[ i * 4 ] >> 5, items[ i * 4 ], items[ i * 4 + 1 ], uint16_t ( items[ i * 4 + 2 ] << 8 | items[ i * 4 + 3 ] ) );
	}

	void flush () override {}
	void resetRings () override {}
	void mute ( bool ) override {}
	void resetRegisters () override {}
	void setClock ( long ) override {}

private:
	int		sids;
};

struct Sunk
{
	uint8_t		chip, reg, val;
	int64_t		cycle;
};

std::vector<Sunk>	sunk;

void collect ( void*, const uint8_t chip, const uint8_t reg, const uint8_t val, const int64_t cycle ) noexcept
{
	// Main test thread only ever reads this after the emulation stopped
	sunk.push_back ( { chip, reg, val, cycle } );
}

bool checkTune ( const char* path, const int seconds )
{
	std::printf ( "%s\n", path );

	sunk.clear ();

	SidConfig	cfg;
	libsidplayfp::Player	player;
	SidTune		tune ( path );

	tune.selectSong ( 0 );

	if ( ! player.setConfig ( cfg ) || ! player.loadTune ( &tune ) )
	{
		std::printf ( "  cannot load\n" );
		return false;
	}

	const auto	chips = player.getNumChips ();
	auto		rec = std::make_unique<Recorder> ( chips );
	auto*		recorder = rec.get ();

	std::atomic<int64_t>	playhead { -100000 };
	HardwareOutput			hw;
	hw.open ( std::move ( rec ) );
	hw.setPlayheadSource ( [ &playhead ] { return playhead.load (); } );

	uint8_t	regs[ 8 ][ 32 ] = {};

	for ( auto c = 0; c < chips; ++c )
		player.getSidStatus ( c, regs[ c ] );

	const auto	cycle0 = player.getCycleTime ();
	const auto	hz = player.getCpuFrequency ();

	if ( ! hw.arm ( regs, chips, cycle0, hz ) )
	{
		std::printf ( "  arm failed\n" );
		return false;
	}

	// Two consumers of the same events: the output and a plain log for the comparison
	static struct Fanout
	{
		HardwareOutput*	out = nullptr;
	} fan;
	fan.out = &hw;

	player.setWriteSink ( { []( void* user, uint8_t chip, uint8_t reg, uint8_t val, int64_t cycle ) noexcept
	{
		collect ( nullptr, chip, reg, val, cycle );
		HardwareOutput::onWrite ( static_cast<Fanout*> ( user )->out, chip, reg, val, cycle );
	}, &fan } );

	const auto	start = std::chrono::steady_clock::now ();
	auto		rendered = int64_t ( 0 );
	std::vector<float>	left ( 735 );

	for ( auto f = 0; f < seconds * 60; ++f )
	{
		for ( ;; )
		{
			const auto	ms = std::chrono::duration_cast<std::chrono::milliseconds> ( std::chrono::steady_clock::now () - start ).count ();
			playhead = ms * 44100 / 1000;

			if ( rendered - playhead <= 6615 )
				break;

			std::this_thread::sleep_for ( std::chrono::milliseconds ( 2 ) );
		}

		player.play ( left, {}, {} );
		hw.setEmulatedCycle ( player.getCycleTime () );
		rendered += 735;
	}

	// Let the writer finish what the last frames put in reach
	playhead = rendered + 44100;
	std::this_thread::sleep_for ( std::chrono::milliseconds ( 100 ) );

	player.setWriteSink ( {} );
	const auto	status = hw.status ();
	const auto	lastCycle = sunk.empty () ? cycle0 : sunk.back ().cycle;

	// The recorder belongs to the output, read it before the output goes away
	std::vector<Write>	got;
	{
		const std::lock_guard	lk ( recorder->lock );
		got = recorder->writes;
	}

	hw.close ();

	bool	ok = true;

	// Snapshot writes come first
	const size_t	snapshot = size_t ( chips ) * 25;

	if ( got.size () < snapshot )
	{
		std::printf ( "  FAIL fewer writes than the snapshot\n" );
		return false;
	}

	// Forwarded stream: every emulated write to regs $00-$18, in order, fillers excluded
	std::vector<Sunk>	expect;
	for ( const auto& e : sunk )
		if ( e.reg <= 0x18 && e.chip < chips )
			expect.push_back ( e );

	size_t	k = 0;
	int64_t	timeline = cycle0 + int64_t ( snapshot ) * 16;
	size_t	offTimeline = 0;
	int64_t	worst = 0;

	for ( size_t i = snapshot; i < got.size (); ++i )
	{
		const auto&	w = got[ i ];
		timeline += w.cycles;

		// Fillers rewrite the volume with the full 65535 delta, the unmute restores it with 16
		if ( ( w.cycles == 65535 || w.cycles == 16 ) && w.reg % 0x20 == 0x18 && ( k >= expect.size () || expect[ k ].cycle != timeline ) )
			continue;

		if ( k >= expect.size () )
		{
			std::printf ( "  FAIL extra forwarded write\n" );
			ok = false;
			break;
		}

		const auto&	e = expect[ k ];

		if ( w.sid != e.chip || w.reg != ( e.reg & 0x1f ) || w.val != e.val )
		{
			std::printf ( "  FAIL write %zu differs: got sid %d reg %02x val %02x cyc %u, expected chip %d reg %02x val %02x cycle %lld (timeline %lld)\n", k, w.sid, w.reg, w.val, w.cycles, e.chip, e.reg, e.val, (long long)e.cycle, (long long)timeline );
			ok = false;
			break;
		}

		// The board's time at this write must be the emulated cycle, except right after the
		// snapshot, which needs a little time of its own
		if ( e.cycle - cycle0 > 2000 && timeline != e.cycle )
		{
			++offTimeline;
			worst = std::max ( worst, std::abs ( timeline - e.cycle ) );
		}

		++k;
	}

	if ( offTimeline )
	{
		std::printf ( "  FAIL %zu writes off the emulated timeline, worst %lld cycles\n", offTimeline, (long long)worst );
		ok = false;
	}

	if ( k != expect.size () )
	{
		std::printf ( "  FAIL forwarded %zu of %zu writes\n", k, expect.size () );
		ok = false;
	}

	const auto	emulated = double ( lastCycle - cycle0 ) / hz;

	std::printf ( "  chips %d, %zu writes forwarded, dropped %llu, stalls %llu, lag %.1f ms\n", chips, k,
				  (unsigned long long)status.dropped, (unsigned long long)status.stalls, status.lagMs );
	std::printf ( "  %.2f emulated seconds at %.0f Hz, every write on the emulated timeline\n", emulated, hz );

	if ( status.dropped != 0 )
	{
		std::printf ( "  FAIL dropped writes\n" );
		ok = false;
	}

	std::printf ( "  %s\n", ok ? "ok" : "FAILED" );

	return ok;
}

}

int main ( int argc, char** argv )
{
	if ( argc < 2 )
	{
		std::printf ( "usage: hardwareoutput_tune_test <tune.sid>...\n" );
		return 1;
	}

	bool	ok = true;

	for ( auto i = 1; i < argc; ++i )
		ok = checkTune ( argv[ i ], 5 ) && ok;

	return ok ? 0 : 1;
}
