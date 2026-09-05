#include <JuceHeader.h>

#include <map>

#include "TunePatches.h"

//-----------------------------------------------------------------------------

namespace
{
	struct Patch
	{
		uint32_t	offset;
		uint8_t		from;
		uint8_t		to;
	};

	// Keyed by collection path without the leading slash
	std::map<juce::String, std::vector<Patch>>& patches ()
	{
		static std::map<juce::String, std::vector<Patch>>	p;
		return p;
	}

	std::map<juce::String, juce::String>& lengthOverrides ()
	{
		static std::map<juce::String, juce::String>	l;
		return l;
	}

	[[ nodiscard ]] juce::String stripLeadingSlash ( const juce::String& path )
	{
		return path.startsWithChar ( '/' ) ? path.substring ( 1 ) : path;
	}

	[[ nodiscard ]] bool looksLikeTime ( const juce::String& token )
	{
		return token.containsChar ( ':' ) && token.containsOnly ( "0123456789:." );
	}
}
//-----------------------------------------------------------------------------

void tunepatches::load ( const juce::String& text )
{
	auto&	all = patches ();
	auto&	lengths = lengthOverrides ();

	all.clear ();
	lengths.clear ();

	std::vector<Patch>*	current = nullptr;

	for ( const auto& raw : juce::StringArray::fromLines ( text ) )
	{
		const auto	line = raw.trim ();

		if ( line.isEmpty () || line.startsWithChar ( '#' ) )
			continue;

		const auto	tokens = juce::StringArray::fromTokens ( line, " \t", "" );

		// "/path/Tune.sid [m:ss ...]", the times (one per subtune) replace the
		// tune's song lengths, anything after them is a comment
		if ( line.startsWithChar ( '/' ) )
		{
			current = &all[ tokens[ 0 ].substring ( 1 ) ];

			juce::StringArray	times;

			for ( auto i = 1; i < tokens.size () && looksLikeTime ( tokens[ i ] ); ++i )
				times.add ( tokens[ i ] );

			if ( ! times.isEmpty () )
				lengths[ tokens[ 0 ] ] = times.joinIntoString ( " " );

			continue;
		}

		// "offset old new", anything after the third token is a comment
		const auto	hex = [ &tokens ] ( int i ) { return tokens.size () > i && tokens[ i ].containsOnly ( "0123456789abcdefABCDEF" ); };

		if ( current == nullptr || ! hex ( 0 ) || ! hex ( 1 ) || ! hex ( 2 ) )
		{
			Z_WARN ( "tune-patches: bad line \"" << line << "\"" );
			continue;
		}

		current->push_back ( { uint32_t ( tokens[ 0 ].getHexValue32 () ), uint8_t ( tokens[ 1 ].getHexValue32 () ), uint8_t ( tokens[ 2 ].getHexValue32 () ) } );
	}
}
//-----------------------------------------------------------------------------

const std::map<juce::String, juce::String>& tunepatches::lengths ()
{
	return lengthOverrides ();
}
//-----------------------------------------------------------------------------

bool tunepatches::apply ( const char* collectionPath, std::vector<uint8_t>& bytes )
{
	const auto&	all = patches ();

	if ( all.empty () )
		return false;

	const auto	it = all.find ( stripLeadingSlash ( collectionPath ) );

	if ( it == all.end () )
		return false;

	// All or nothing: a file the patch wasn't written for (an HVSC release that
	// already carries the fix, typically) stays untouched
	for ( const auto& p : it->second )
	{
		if ( p.offset >= bytes.size () || bytes[ p.offset ] != p.from )
		{
			Z_WARN ( "tune-patches: " << collectionPath << " doesn't match at offset " << juce::String::toHexString ( int ( p.offset ) ) << ", loaded unpatched" );
			return false;
		}
	}

	for ( const auto& p : it->second )
		bytes[ p.offset ] = p.to;

	Z_INFO ( "tune-patches: patched " << collectionPath );

	return true;
}
//-----------------------------------------------------------------------------
