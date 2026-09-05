#pragma once
// Shared helpers for the Tests/ tools: file slurping, 32-bit float WAV
// read/write (interleaved, also listenable in any editor when a diff needs
// ears), and the perf-baseline text file ("name<TAB>seconds<TAB>elapsed").

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace testcommon
{
	namespace fs = std::filesystem;

	inline std::string slurp ( const fs::path& p )
	{
		std::ifstream	f ( p, std::ios::binary );
		std::stringstream	s;
		s << f.rdbuf ();
		return s.str ();
	}

	inline std::vector<char> slurpBytes ( const fs::path& p )
	{
		const auto	str = slurp ( p );
		return { str.begin (), str.end () };
	}

	//-------------------------------------------------------------------------

	inline bool writeWavF32 ( const fs::path& path, const float* L, const float* R, const uint32_t frames, const int sampleRate )
	{
		const uint32_t	channels = R ? 2 : 1;
		const uint32_t	dataBytes = frames * channels * 4;

		std::ofstream	f ( path, std::ios::binary );
		if ( !f )
			return false;

		const auto	u32 = [ &f ] ( uint32_t v ) { f.write ( reinterpret_cast<const char*>( &v ), 4 ); };
		const auto	u16 = [ &f ] ( uint16_t v ) { f.write ( reinterpret_cast<const char*>( &v ), 2 ); };

		f.write ( "RIFF", 4 ); u32 ( 4 + 26 + 12 + 8 + dataBytes ); f.write ( "WAVE", 4 );
		f.write ( "fmt ", 4 ); u32 ( 18 );
		u16 ( 3 );	// WAVE_FORMAT_IEEE_FLOAT
		u16 ( static_cast<uint16_t>( channels ) );
		u32 ( static_cast<uint32_t>( sampleRate ) );
		u32 ( static_cast<uint32_t>( sampleRate ) * channels * 4 );
		u16 ( static_cast<uint16_t>( channels * 4 ) );
		u16 ( 32 );
		u16 ( 0 );	// cbSize
		f.write ( "fact", 4 ); u32 ( 4 ); u32 ( frames );
		f.write ( "data", 4 ); u32 ( dataBytes );

		for ( uint32_t i = 0; i < frames; ++i )
		{
			f.write ( reinterpret_cast<const char*>( &L[ i ] ), 4 );
			if ( R )
				f.write ( reinterpret_cast<const char*>( &R[ i ] ), 4 );
		}
		return static_cast<bool>( f );
	}

	struct Wav
	{
		int	channels = 0;
		std::vector<float>	samples;	// interleaved
		uint32_t frames () const { return channels ? static_cast<uint32_t>( samples.size () / channels ) : 0; }
	};

	inline bool readWavF32 ( const fs::path& path, Wav& out )
	{
		const auto	bytes = slurpBytes ( path );
		if ( bytes.size () < 12 || memcmp ( bytes.data (), "RIFF", 4 ) != 0 || memcmp ( bytes.data () + 8, "WAVE", 4 ) != 0 )
			return false;

		size_t	pos = 12;
		int	channels = 0, bits = 0, format = 0;
		while ( pos + 8 <= bytes.size () )
		{
			char	id[ 5 ] = {};
			memcpy ( id, bytes.data () + pos, 4 );
			uint32_t	len;
			memcpy ( &len, bytes.data () + pos + 4, 4 );
			pos += 8;
			if ( pos + len > bytes.size () )
				return false;

			if ( memcmp ( id, "fmt ", 4 ) == 0 && len >= 16 )
			{
				uint16_t	fmt16, ch16, bits16;
				memcpy ( &fmt16, bytes.data () + pos, 2 );
				memcpy ( &ch16, bytes.data () + pos + 2, 2 );
				memcpy ( &bits16, bytes.data () + pos + 14, 2 );
				format = fmt16; channels = ch16; bits = bits16;
			}
			else if ( memcmp ( id, "data", 4 ) == 0 )
			{
				if ( format != 3 || bits != 32 || channels < 1 )
					return false;
				out.channels = channels;
				out.samples.resize ( len / 4 );
				memcpy ( out.samples.data (), bytes.data () + pos, out.samples.size () * 4 );
				return true;
			}
			pos += len + ( len & 1 );
		}
		return false;
	}

	//-------------------------------------------------------------------------
	// perf baseline: one line per entry, "name<TAB>renderSeconds<TAB>elapsedSeconds"

	inline std::map<std::string, std::pair<int, double>> readBaseline ( const fs::path& p )
	{
		std::map<std::string, std::pair<int, double>>	m;
		std::ifstream	f ( p );
		std::string	line;
		while ( std::getline ( f, line ) )
		{
			const auto	t1 = line.find ( '\t' );
			const auto	t2 = line.find ( '\t', t1 + 1 );
			if ( t1 == std::string::npos || t2 == std::string::npos )
				continue;
			m[ line.substr ( 0, t1 ) ] = { std::stoi ( line.substr ( t1 + 1, t2 - t1 - 1 ) ),
										   std::stod ( line.substr ( t2 + 1 ) ) };
		}
		return m;
	}

	inline void writeBaseline ( const fs::path& p, const std::map<std::string, std::pair<int, double>>& m )
	{
		std::ofstream	f ( p, std::ios::trunc );
		for ( const auto& [ name, v ] : m )
			f << name << '\t' << v.first << '\t' << v.second << '\n';
	}
}
