#include <JuceHeader.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "Similarity.h"
#include "Config/FilePaths.h"

//-----------------------------------------------------------------------------

int Similarity::load ( const juce::MemoryBlock& mb, const Database& database )
{
	dims = 0;
	codebook = {};
	rows = {};
	codes = {};
	tuneRows = {};

	const auto	src = (const uint8_t*)mb.getData ();
	const auto	size = mb.getSize ();
	size_t		at = 0;

	auto has = [ & ] ( const size_t n )	{	return at + n <= size;	};
	auto get_u16 = [ & ]	{	uint16_t v;	std::memcpy ( &v, src + at, 2 );	at += 2;	return v;	};
	auto get_u32 = [ & ]	{	uint32_t v;	std::memcpy ( &v, src + at, 4 );	at += 4;	return v;	};

	auto broken = [ this ]
	{
		codebook = {};
		rows = {};
		codes = {};
		tuneRows = {};

		return 0;
	};

	if ( ! has ( 5 ) || std::memcmp ( src, "uSIM", 4 ) )
		return 0;
	at = 4;

	const auto	fileDims = int ( src[ at++ ] );
	const auto	codebookBytes = size_t ( fileDims ) * centroids * sizeof ( float );
	if ( fileDims == 0 || ! has ( codebookBytes + 4 ) )
		return 0;

	codebook.resize ( size_t ( fileDims ) * centroids );
	std::memcpy ( codebook.data (), src + at, codebookBytes );
	at += codebookBytes;

	auto	numTunes = get_u32 ();

	rows.reserve ( numTunes * 3 / 2 );
	codes.reserve ( rows.capacity () * size_t ( fileDims ) );

	std::string	key;

	while ( numTunes-- )
	{
		if ( ! has ( 1 ) )
			return broken ();

		const auto	pathLen = size_t ( src[ at++ ] );
		if ( ! has ( pathLen + 2 ) )
			return broken ();

		key.assign ( filepaths::hvscMarker );
		key.append ( (const char*)src + at, pathLen );
		key.append ( ".sid" );
		at += pathLen;

		const auto	numRows = size_t ( get_u16 () );
		if ( ! has ( numRows * ( 2 + size_t ( fileDims ) ) ) )
			return broken ();

		// Tunes the database lacks are skipped, their rows would have no entry to show
		const auto	entry = database.findEntry ( key );
		const auto	firstRow = uint32_t ( rows.size () );

		for ( auto i = size_t ( 0 ); i < numRows; ++i )
		{
			const auto	subtune = int ( get_u16 () );

			if ( entry && subtune >= 1 && subtune <= entry->numTunes )
			{
				auto	norm = 0.0;
				for ( auto d = 0; d < fileDims; ++d )
				{
					const auto	v = double ( codebook[ size_t ( d ) * centroids + src[ at + size_t ( d ) ] ] );
					norm += v * v;
				}

				rows.push_back ( { entry, subtune, norm > 0.0 ? float ( 1.0 / std::sqrt ( norm ) ) : 0.0f } );
				codes.insert ( codes.end (), src + at, src + at + fileDims );
			}

			at += size_t ( fileDims );
		}

		if ( rows.size () > firstRow )
			tuneRows[ entry->lowerFile ] = { firstRow, uint32_t ( rows.size () ) - firstRow };
	}

	dims = fileDims;

	return int ( rows.size () );
}
//-----------------------------------------------------------------------------

bool Similarity::contains ( const Database::entry& entry ) const
{
	return tuneRows.contains ( entry.lowerFile );
}
//-----------------------------------------------------------------------------

void Similarity::decode ( const size_t rowIndex, float* dst ) const
{
	const auto	rowCodes = codes.data () + rowIndex * size_t ( dims );
	const auto	invNorm = rows[ rowIndex ].invNorm;

	for ( auto d = 0; d < dims; ++d )
		dst[ d ] = codebook[ size_t ( d ) * centroids + rowCodes[ d ] ] * invNorm;
}
//-----------------------------------------------------------------------------

std::vector<Similarity::match> Similarity::find ( const std::string_view lowerKey, int subtune, const int count,
												  const std::function<bool ( const match& seed, const match& candidate )>& accept ) const
{
	// Re-rips and previews of one song measured 0.999
	constexpr auto	nearCopy = 0.98f;

	const auto	it = tuneRows.find ( lowerKey );
	if ( it == tuneRows.end () )
		return {};

	const auto [ firstRow, numRows ] = it->second;
	const auto	seedEntry = rows[ firstRow ].entry;

	if ( subtune == 0 )
		subtune = seedEntry->startTune;

	auto	seedRow = firstRow;
	while ( seedRow < firstRow + numRows && rows[ seedRow ].subtune != subtune )
		++seedRow;

	if ( seedRow == firstRow + numRows )
		return {};

	const auto	numDims = size_t ( dims );

	// The vectors of the seed and every match so far, for the near-copy test
	std::vector<float>	picked ( numDims );
	decode ( seedRow, picked.data () );

	// Each code's share of the dot product with the seed, so a row scores in
	// dims table lookups
	std::vector<float>	table ( numDims * centroids );
	for ( auto d = size_t ( 0 ); d < numDims; ++d )
		for ( auto c = size_t ( 0 ); c < centroids; ++c )
			table[ d * centroids + c ] = picked[ d ] * codebook[ d * centroids + c ];

	std::vector<std::pair<float, uint32_t>>	scored;
	scored.reserve ( rows.size () );

	const auto	tablePtr = table.data ();
	auto		rowCodes = codes.data ();

	for ( auto i = uint32_t ( 0 ); i < uint32_t ( rows.size () ); ++i, rowCodes += numDims )
	{
		if ( rows[ i ].entry == seedEntry )
			continue;

		auto	dot = 0.0f;
		for ( auto d = size_t ( 0 ); d < numDims; ++d )
			dot += tablePtr[ d * centroids + rowCodes[ d ] ];

		scored.emplace_back ( dot * rows[ i ].invNorm, i );
	}

	// A heap, so only the rows actually looked at get ordered
	std::ranges::make_heap ( scored );

	std::vector<match>					result;
	std::vector<const Database::entry*>	used;
	std::vector<float>					candidate ( numDims );

	while ( ! scored.empty () && int ( result.size () ) < count )
	{
		std::ranges::pop_heap ( scored );
		const auto	rowIndex = scored.back ().second;
		scored.pop_back ();

		const auto&	r = rows[ rowIndex ];

		if ( std::ranges::contains ( used, r.entry ) || ! accept ( { seedEntry, subtune }, { r.entry, r.subtune } ) )
			continue;

		decode ( rowIndex, candidate.data () );

		auto	isNearCopy = false;
		for ( auto p = picked.data (); p < picked.data () + picked.size () && ! isNearCopy; p += numDims )
		{
			auto	dot = 0.0f;
			for ( auto d = size_t ( 0 ); d < numDims; ++d )
				dot += p[ d ] * candidate[ d ];

			isNearCopy = dot > nearCopy;
		}

		if ( isNearCopy )
			continue;

		used.push_back ( r.entry );
		picked.insert ( picked.end (), candidate.begin (), candidate.end () );
		result.push_back ( { r.entry, r.subtune } );
	}

	return result;
}
//-----------------------------------------------------------------------------
