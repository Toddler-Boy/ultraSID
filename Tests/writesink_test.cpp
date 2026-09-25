// libSidplayEZ write sink test, no JUCE and no hardware.
//
// For each tune given on the command line: load it, snapshot every chip's registers, install a
// write sink, render N seconds, then check that
//  - replaying the sink log on top of the snapshot reproduces the final getSidStatus () state
//  - the log has one chip index per emulated chip, cycles never go backwards
//  - the rendered audio is bit-identical with and without a sink installed
//  - the sink survives a chip recreation (setConfig () with a changed config, loadTune ())
//
//   Tests/writesink_test.sh <tune.sid>...
//
// Exit 0: all checks pass. Exit 1: a check failed or a tune did not load.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "../Source/libSidplayEZ/src/player.h"
#include "../Source/libSidplayEZ/src/sidplayfp/SidTune.h"

namespace
{

constexpr int	kSampleRate = 44100;
constexpr int	kChunk = 735;		// One 60 Hz frame of samples
constexpr int	kMaxChips = 8;

struct Event
{
	uint8_t		chip;
	uint8_t		reg;
	uint8_t		val;
	int64_t		cycle;
};

struct Log
{
	std::vector<Event>	events;
};

void onWrite ( void* user, uint8_t chip, uint8_t reg, uint8_t val, int64_t cycle ) noexcept
{
	static_cast<Log*> ( user )->events.push_back ( { chip, reg, val, cycle } );
}

struct Rendered
{
	std::vector<float>		audio;
	std::vector<uint8_t>	final[ kMaxChips ];
	int						chips = 0;
};

// Render 'frames' 60 Hz chunks, optionally with a sink. Snapshot is taken right before the first chunk.
bool render ( const char* path, const int frames, Log* log, uint8_t snapshot[ kMaxChips ][ 32 ], Rendered& out )
{
	SidConfig cfg;
	cfg.frequency = kSampleRate;

	auto player = std::make_unique<libsidplayfp::Player> ();
	auto tune = std::make_unique<SidTune> ( path );

	if ( ! tune->getStatus () )
	{
		std::printf ( "  cannot load %s\n", path );
		return false;
	}

	tune->selectSong ( 0 );

	if ( ! player->setConfig ( cfg ) || ! player->loadTune ( tune.get () ) )
	{
		std::printf ( "  setConfig/loadTune failed: %s\n", player->error () );
		return false;
	}

	out.chips = player->getNumChips ();

	for ( int c = 0; c < out.chips && c < kMaxChips; ++c )
		player->getSidStatus ( c, snapshot[ c ] );

	if ( log )
		player->setWriteSink ( { onWrite, log } );

	std::vector<float>	left ( kChunk );

	for ( int f = 0; f < frames; ++f )
	{
		player->play ( left, {}, {} );
		out.audio.insert ( out.audio.end (), left.begin (), left.end () );
	}

	for ( int c = 0; c < out.chips && c < kMaxChips; ++c )
	{
		out.final[ c ].resize ( 32 );
		player->getSidStatus ( c, out.final[ c ].data () );
	}

	return true;
}

// The sink must still fire after loadTune () recreated the chips.
bool survivesRecreation ( const char* path )
{
	SidConfig cfg;
	cfg.frequency = kSampleRate;

	libsidplayfp::Player	player;
	SidTune					tune ( path );
	Log						log;

	tune.selectSong ( 0 );

	if ( ! player.setConfig ( cfg ) || ! player.loadTune ( &tune ) )
		return false;

	player.setWriteSink ( { onWrite, &log } );

	cfg.useFilter = ! cfg.useFilter;

	if ( ! player.setConfig ( cfg, true ) || ! player.loadTune ( &tune ) )
		return false;

	std::vector<float>	left ( kChunk );
	player.play ( left, {}, {} );

	return ! log.events.empty ();
}

bool checkTune ( const char* path, const int seconds )
{
	std::printf ( "%s\n", path );

	const int	frames = seconds * 60;

	static uint8_t	snapA[ kMaxChips ][ 32 ];
	static uint8_t	snapB[ kMaxChips ][ 32 ];

	Rendered	plain, sunk;
	Log			log;

	if ( ! render ( path, frames, nullptr, snapA, plain ) || ! render ( path, frames, &log, snapB, sunk ) )
		return false;

	bool	ok = true;

	// Audio must be untouched by the sink
	if ( plain.audio.size () != sunk.audio.size () || std::memcmp ( plain.audio.data (), sunk.audio.data (), plain.audio.size () * sizeof ( float ) ) != 0 )
	{
		std::printf ( "  FAIL audio differs with sink installed\n" );
		ok = false;
	}

	// Chip indices in range, cycles non-decreasing
	size_t	perChip[ kMaxChips ] = {};
	int64_t	lastCycle = -1;
	size_t	backwards = 0;

	for ( const auto& e : log.events )
	{
		if ( e.chip >= sunk.chips )
		{
			std::printf ( "  FAIL chip index %d out of range (%d chips)\n", e.chip, sunk.chips );
			ok = false;
			break;
		}

		++perChip[ e.chip ];

		if ( e.cycle < lastCycle )
			++backwards;

		lastCycle = e.cycle;
	}

	if ( backwards )
	{
		std::printf ( "  FAIL %zu writes with a cycle earlier than the previous write\n", backwards );
		ok = false;
	}

	// Snapshot + log replay must equal the final state, write-only registers $00-$18
	uint8_t	shadow[ kMaxChips ][ 32 ];
	std::memcpy ( shadow, snapB, sizeof shadow );

	for ( const auto& e : log.events )
		if ( e.chip < kMaxChips && e.reg < 0x19 )
			shadow[ e.chip ][ e.reg ] = e.val;

	for ( int c = 0; c < sunk.chips && c < kMaxChips; ++c )
		for ( int r = 0; r < 0x19; ++r )
			if ( shadow[ c ][ r ] != sunk.final[ c ][ r ] )
			{
				std::printf ( "  FAIL chip %d reg $%02x: replay %02x, status %02x\n", c, r, shadow[ c ][ r ], sunk.final[ c ][ r ] );
				ok = false;
			}

	std::printf ( "  chips %d, %d frames, %zu writes (%.1f per frame)\n", sunk.chips, frames, log.events.size (), double ( log.events.size () ) / frames );

	for ( int c = 0; c < sunk.chips && c < kMaxChips; ++c )
		std::printf ( "    chip %d: %zu writes\n", c, perChip[ c ] );

	if ( ! survivesRecreation ( path ) )
	{
		std::printf ( "  FAIL sink lost after chip recreation\n" );
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
		std::printf ( "usage: writesink_test <tune.sid>...\n" );
		return 1;
	}

	bool	ok = true;

	for ( int i = 1; i < argc; ++i )
		ok = checkTune ( argv[ i ], 10 ) && ok;

	return ok ? 0 : 1;
}
