#include <algorithm>
#include <cctype>
#include <ranges>

#include "STIL_Quotes.h"

#include "std_lime/lime_string_utils.h"

#include "ultra-shared/Helpers/Regex.h"

namespace stilq
{

static bool isDigitC ( unsigned char c ) { return c >= '0' && c <= '9'; }
static bool isUpperC ( unsigned char c ) { return ( c >= 'A' && c <= 'Z' ) || ( c >= 0xC0 && c <= 0xDE && c != 0xD7 ); }
static bool isLetterC ( unsigned char c ) { return std::isalpha ( c ) || c >= 0xC0; }

// Does the paren content look like a person/handle, not a note?
// Shape-based: no fixed list of names, only STIL's structural conventions
// (subtune markers, (C), translations, source notes) are rejected.
// Returns the cleaned speaker name, or "" if it is not a speaker.
static std::string speakerOf ( std::string s )
{
    // collapse internal whitespace (line-merge preprocessing may leave
    // newlines inside a wrapped "(Comment by\nName)")
    std::string t; bool ws = false;
    for ( unsigned char c : s )
    {
        if ( c == ' ' || c == '\n' || c == '\t' || c == '\r' ) { ws = !t.empty (); continue; }
        if ( ws ) { t += ' '; ws = false; }
        t += (char)c;
    }
    s = std::move ( t );

    if ( lime::str::toLower ( s.substr ( 0, 11 ) ) == "comment by " )
        s = s.substr ( 11 );
    else if ( lime::str::toLower ( s.substr ( 0, 3 ) ) == "by " )
        s = s.substr ( 3 );

    if ( s.empty () || s.size () > 45 ) return {};

    if ( std::ranges::any_of ( s, [] ( unsigned char c ) { return isDigitC ( c ) || c == '"'; } ) )
        return {};   // (#1), (C) 1984, years

    const auto ls = lime::str::toLower ( s );
    if ( ls.contains ( "www." ) || ls.contains ( "http" ) || ls.contains ( ".com" ) )
        return {};

    std::vector<std::string_view> tokens;
    for ( auto&& r : std::views::split ( std::string_view ( s ), ' ' ) )
        if ( std::string_view v ( r ); !v.empty () )
            tokens.push_back ( v );
    if ( tokens.empty () || tokens.size () > 4 ) return {};

    auto barewordOf = [] ( std::string_view v )     // lowercased, trailing ./: stripped
    {
        std::string w = lime::str::toLower ( std::string ( v ) );
        while ( !w.empty () && ( w.back () == '.' || w.back () == ':' ) ) w.pop_back ();
        return w;
    };

    // a paren whose last word is "tune" describes a subtune, not a speaker
    if ( !tokens.empty () && barewordOf ( tokens.back () ) == "tune" )
        return {};

    // reject note-style first words (STIL conventions, not names)
    static constexpr std::string_view firstBlack[] = {
        "translated", "translation", "quote", "quoted", "quotes", "info",
        "also", "see", "aka", "a.k.a", "e.g", "i.e", "from", "or", "the", "copyright" };
    if ( std::ranges::contains ( firstBlack, barewordOf ( tokens.front () ) ) )
        return {};

    if ( tokens.size () == 1 )
    {
        const auto tok = tokens.front ();
        if ( !isLetterC ( (unsigned char)tok.front () ) ) return {};
        if ( !std::ranges::all_of ( tok, [] ( unsigned char c )
        { return isLetterC ( c ) || c == '-' || c == '\'' || c == '&' || c == '+' || c == '_'; } ) )
            return {};
        const auto w = lime::str::toLower ( std::string ( tok ) );
        return ( w != "c" && w != "ntsc" && w != "pal" ) ? s : std::string ();
    }

    // multi-token: each token capitalized, or a name particle
    static constexpr std::string_view particles[] = {
        "van", "de", "der", "von", "den", "ter", "la", "le", "di", "da", "of" };
    if ( !std::ranges::all_of ( tokens, [ & ] ( std::string_view v )
    { return isUpperC ( (unsigned char)v.front () )
                                || std::ranges::contains ( particles, barewordOf ( v ) ); } ) )
        return {};

    while ( !s.empty () && s.back () == '.' ) s.pop_back ();
    return s;
}

// Is the quote at position q preceded (ignoring whitespace) by c1 or c2?
static bool afterChar ( const std::string& s, size_t q, char c1, char c2 )
{
    size_t k = q;
    while ( k > 0 && ( s[ k - 1 ] == ' ' || s[ k - 1 ] == '\n' || s[ k - 1 ] == '\t' || s[ k - 1 ] == '\r' ) ) --k;
    return k > 0 && ( s[ k - 1 ] == c1 || s[ k - 1 ] == c2 );
}

// All '"' positions in [from, to)
static std::vector<size_t> quotePositions ( const std::string& s, size_t from, size_t to )
{
    std::vector<size_t> v;
    for ( size_t i = from; i < to && i < s.size (); ++i )
        if ( s[ i ] == '"' ) v.push_back ( i );
    return v;
}

// Given quote positions and the index (into qpos) of the closing quote,
// pick the opening quote: candidates have an EVEN number of quotes strictly
// between them and the close (nested pairs balance out). Prefer the farthest
// candidate that sits at segment start or is preceded by ": " or ", ";
// otherwise the nearest balanced one.
static size_t pickOpening ( const std::string& s, const std::vector<size_t>& qpos,
                           size_t closeIdx, size_t segStart )
{
    // a quote glued to a word character ('breathe"') is a closing quote and
    // can never open a quotation
    auto canOpen = [ &s ] ( size_t q )
    {
        if ( q == 0 ) return true;
        const unsigned char c = (unsigned char)s[ q - 1 ];
        return !( std::isalnum ( c ) || c >= 0xC0 );
    };

    std::vector<size_t> cands;
    for ( size_t k = 0; k < closeIdx; ++k )
        if ( canOpen ( qpos[ k ] ) && ( closeIdx - k - 1 ) % 2 == 0 )   // even quote count between
            cands.push_back ( qpos[ k ] );
    if ( cands.empty () )
        for ( size_t k = 0; k < closeIdx; ++k )
            if ( canOpen ( qpos[ k ] ) ) cands.push_back ( qpos[ k ] );
    if ( cands.empty () )
        for ( size_t k = 0; k < closeIdx; ++k ) cands.push_back ( qpos[ k ] );

    const auto pref = std::ranges::find_if ( cands, [ & ] ( size_t q )   // ascending = farthest back first
    { return q == segStart || afterChar ( s, q, ':', ',' ); } );
    return pref != cands.end () ? *pref : cands.back ();            // else nearest balanced
}

std::vector<StilSegment> splitCommentQuotes ( const std::string& bodyIn )
{
    const std::string body = std::string { lime::str::trim ( bodyIn ) };
    std::vector<StilSegment> out;

    // A blank line follows an interview question, whatever the answer marker
    // style (A:, MG:, none)
    static const regex::Pattern qaBlankLine ( R"((^|\n)([ \t]*Q\s*:[^\n]{0,220}\?)[ \t]*\n(?!\n))" );

    auto emit = [ &out ] ( const char* type, std::string t, std::string speaker = {} )
    {
        t = qaBlankLine.replaceAll ( t, [] ( const std::vector<std::string>& g )
        { return g[ 1 ] + g[ 2 ] + "\n\n"; } );

        t = std::string { lime::str::trim ( t ) };
        if ( !t.empty () ) out.push_back ( { type, std::move ( t ), std::move ( speaker ) } );
    };

    size_t cursor = 0;
    bool foundAny = false;

    // ---- pass 1: attribution-anchored quotes:  " [ .]{0,2} ( speaker ) ----
    for ( size_t i = 0; i < body.size (); ++i )
    {
        if ( body[ i ] != '"' ) continue;

        size_t j = i + 1, dots = 0;
        while ( j < body.size () )
        {
            const char c = body[ j ];
            if ( c == ' ' || c == '\n' || c == '\t' || c == '\r' ) { ++j; continue; }
            if ( c == '.' && dots < 2 ) { ++j; ++dots; continue; }
            break;
        }
        if ( j >= body.size () || body[ j ] != '(' ) continue;

        const size_t close = body.find ( ')', j + 1 );
        if ( close == std::string::npos || close - j > 61 ) continue;
        std::string speaker;
        bool weakTier = false;
        {
            const std::string paren = body.substr ( j + 1, close - j - 1 );
            speaker = speakerOf ( paren );
            if ( speaker.empty () )                      // "(RH, in the UK magazine ...)"
            {
                const size_t comma = paren.find ( ',' );
                if ( comma != std::string::npos )
                    speaker = speakerOf ( paren.substr ( 0, comma ) );
            }
            if ( speaker.empty () )                      // "(All comments from JT)"
            {
                weakTier = true;
                const auto lp = lime::str::toLower ( paren );
                size_t at = std::string::npos;
                for ( const char* kw : { " from ", " by " } )
                {
                    const size_t f = lp.rfind ( kw );
                    if ( f != std::string::npos )
                    {
                        const size_t cand = f + std::string ( kw ).size ();
                        if ( at == std::string::npos || cand > at ) at = cand;
                    }
                }
                if ( at != std::string::npos )
                    speaker = speakerOf ( paren.substr ( at ) );
            }
            if ( speaker.empty () ) continue;
        }

        const auto qpos = quotePositions ( body, cursor, i + 1 );   // includes i
        if ( qpos.size () < 2 ) continue;                           // need an opening

        const size_t open = pickOpening ( body, qpos, qpos.size () - 1, cursor );
        if ( weakTier && i - open < 30 ) continue;

        // chain backward across whitespace-adjacent quote spans: an unattributed
        // quote directly preceding an attributed one ("..." "..." (JT)) belongs
        // to the quote list, not the comment. Quoted titles never abut like
        // this, they are separated by punctuation ("Dick Barton". "Monty...).
        std::vector<std::pair<size_t, size_t>> chain;             // (open, close) inclusive
        size_t chainStart = open;
        for ( ;;)
        {
            size_t k = chainStart;
            while ( k > cursor && ( body[ k - 1 ] == ' ' || body[ k - 1 ] == '\n'
                                    || body[ k - 1 ] == '\t' || body[ k - 1 ] == '\r' ) ) --k;
            if ( k <= cursor || body[ k - 1 ] != '"' ) break;

            const auto pq = quotePositions ( body, cursor, k );     // includes k-1
            if ( pq.size () < 2 ) break;
            const size_t po = pickOpening ( body, pq, pq.size () - 1, cursor );
            chain.emplace_back ( po, k - 1 );
            chainStart = po;
        }

        auto comment = body.substr ( cursor, chainStart - cursor );
        std::string lead;

        // An interview lead-in ("Q: ...? A:") right before the quote belongs to
        // the quote block, matching STIL entries that keep it inside the quotes
        static const regex::Pattern leadInPattern ( R"((?:^|\n)\s*(Q\s*:[\s\S]{0,220}?\?\s*(?:A\s*:)?)\s*$)" );
        if ( const auto groups = leadInPattern.capture ( comment ); groups.size () > 1 )
        {
            lead = groups[ 1 ] + " ";
            comment.resize ( comment.rfind ( groups[ 1 ] ) );
        }

        emit ( "COMMENT", comment );

        bool first = true;
        auto emitQuote = [ & ] ( const size_t o, const size_t c )   // enclosing quotes excluded
        {
            emit ( "QUOTE", ( first ? lead : std::string () ) + body.substr ( o + 1, c - o - 1 ), speaker );
            first = false;
        };
        for ( auto it = chain.rbegin (); it != chain.rend (); ++it )
            emitQuote ( it->first, it->second );
        emitQuote ( open, i );
        cursor = close + 1;
        foundAny = true;
        i = close;
    }

    // ---- pass 2 (only if nothing anchored): unattributed quotes ----
    if ( ! foundAny )
    {
        const auto qpos = quotePositions ( body, 0, body.size () );
        if ( qpos.size () >= 2 )
        {
            const size_t F = qpos.front (), L = qpos.back ();
            const bool longSpan = L - F >= 80;

            // whole-comment quote: tiny prefix, tiny suffix
            if ( longSpan && F <= 10 && body.size () - 1 - L <= 2 )
            {
                emit ( "COMMENT", body.substr ( 0, F ) );
                emit ( "QUOTE", body.substr ( F + 1, L - F - 1 ) );
                emit ( "COMMENT", body.substr ( L + 1 ) );
                return out;
            }

            // reporting-colon quote:  ... says/said/story: "..."
            if ( afterChar ( body, F, ':', ':' ) )
            {
                // closing = farthest quote after F with even count between
                size_t closeQ = std::string::npos;
                size_t fIdx = 0; while ( qpos[ fIdx ] != F ) ++fIdx;
                for ( size_t k = qpos.size () - 1; k > fIdx; --k )
                    if ( ( k - fIdx - 1 ) % 2 == 0 ) { closeQ = qpos[ k ]; break; }
                if ( closeQ != std::string::npos && closeQ - F >= 80 )
                {
                    emit ( "COMMENT", body.substr ( 0, F ) );
                    emit ( "QUOTE", body.substr ( F + 1, closeQ - F - 1 ) );
                    emit ( "COMMENT", body.substr ( closeQ + 1 ) );
                    return out;
                }
            }
        }
    }

    emit ( "COMMENT", body.substr ( cursor ) );
    return out;
}

} // namespace stilq
//-----------------------------------------------------------------------------
