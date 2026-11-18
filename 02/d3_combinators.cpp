#include <cassert>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

/* Recursive descent parsers have a very regular structure: sequences
 * become consecutive calls, alternatives become ‹if›s, repetitions
 * become loops. In this demo, we will capture that structure in
 * code: instead of writing each parser by hand, we will «build»
 * parsers out of smaller parsers, using functions called «parser
 * combinators». This style comes from functional programming
 * (Haskell's ‹parsec› is the best-known example), and it is a nice
 * illustration of how close grammars and parsers really are.
 *
 * A «parser» will be any callable object which takes the input (as a
 * ‹std::string_view›) and either fails (returns ‹std::nullopt›), or
 * succeeds and returns a pair: the value it parsed, and the «rest»
 * of the input, which it did not consume. Note that there is no
 * separate lexer here: these parsers work directly on characters. */

template< typename T >
using result = std::optional< std::pair< T, std::string_view > >;

/* The most basic parser accepts a single character, if it satisfies
 * a given predicate. All the other parsers will be built from this
 * one. */

auto satisfy( auto pred )
{
    return [ = ]( std::string_view in ) -> result< char >
    {
        if ( in.empty() || !pred( in[ 0 ] ) )
            return std::nullopt;
        return std::pair{ in[ 0 ], in.substr( 1 ) };
    };
}

auto chr( char c ) { return satisfy( [ = ]( char x ) { return x == c; } ); }
auto digit() { return satisfy( []( char x ) { return x >= '0' && x <= '9'; } ); }

/* ‹map( p, f )› parses the same input as ‹p›, but transforms the
 * value using ‹f›. This is how parsers compute results: e.g. turn a
 * list of digits into a number. */

auto map( auto p, auto f )
{
    return [ = ]( std::string_view in )
        -> result< decltype( f( p( in )->first ) ) >
    {
        if ( auto r = p( in ) )
            return std::pair{ f( r->first ), r->second };
        return std::nullopt;
    };
}

/* ‹seq( p, q )› is the sequence ‹p, q› of EBNF: run ‹p›, then run ‹q›
 * on the rest of the input. The value is the pair of both values.
 * Often, we only care about one of the two values (the other being
 * something like a parenthesis), hence ‹left› and ‹right›. */

auto seq( auto p, auto q )
{
    return [ = ]( std::string_view in )
        -> result< std::pair< decltype( p( in )->first ),
                              decltype( q( in )->first ) > >
    {
        auto a = p( in );
        if ( !a ) return std::nullopt;
        auto b = q( a->second );
        if ( !b ) return std::nullopt;
        return std::pair{ std::pair{ a->first, b->first }, b->second };
    };
}

auto left( auto p, auto q )
{
    return map( seq( p, q ), []( auto pair ) { return pair.first; } );
}

auto right( auto p, auto q )
{
    return map( seq( p, q ), []( auto pair ) { return pair.second; } );
}

/* ‹alt( p, q )› is the alternative ‹p | q›: try ‹p›, and if it
 * fails, try ‹q› «on the same input». This is a form of
 * backtracking, which our hand-written parsers did not need, because
 * they could always decide which branch to take by looking at the
 * next token. Here, we simply try both. */

auto alt( auto p, auto q )
{
    return [ = ]( std::string_view in ) -> decltype( p( in ) )
    {
        if ( auto r = p( in ) )
            return r;
        return q( in );
    };
}

/* ‹many( p )› is the repetition ‹{ p }›: apply ‹p› as many times as
 * it succeeds, collecting the values into a vector. It never fails
 * (zero repetitions are fine). ‹many1› requires at least one. */

auto many( auto p )
{
    return [ = ]( std::string_view in )
        -> result< std::vector< decltype( p( in )->first ) > >
    {
        std::vector< decltype( p( in )->first ) > out;
        while ( auto r = p( in ) )
        {
            out.push_back( r->first );
            in = r->second;
        }
        return std::pair{ out, in };
    };
}

auto many1( auto p )
{
    return [ = ]( std::string_view in ) -> decltype( many( p )( in ) )
    {
        auto r = many( p )( in );
        if ( r->first.empty() )
            return std::nullopt;
        return r;
    };
}

/* Whitespace handling: ‹token( p )› is ‹p› followed by any number of
 * spaces. If every ‘token-level’ parser is wrapped this way, the
 * grammar does not need to mention whitespace anywhere else. */

auto token( auto p ) { return left( p, many( chr( ' ' ) ) ); }

/* Now we can write parsers which look a lot like grammar rules.
 *
 *     number = digit, { digit } ;                                   */

auto number()
{
    auto to_int = []( const std::vector< char > &ds )
    {
        int n = 0;
        for ( char d : ds )
            n = n * 10 + ( d - '0' );
        return n;
    };

    return token( map( many1( digit() ), to_int ) );
}

/*     sep_by( p, s ) = [ p, { s, p } ] ;
 *
 * This is a common pattern (comma-separated lists of things), so it
 * gets a combinator of its own. */

auto sep_by( auto p, auto s )
{
    using T = decltype( p( "" )->first );
    auto cons = []( std::pair< T, std::vector< T > > hd_tl )
    {
        hd_tl.second.insert( hd_tl.second.begin(), hd_tl.first );
        return hd_tl.second;
    };

    auto nonempty = map( seq( p, many( right( s, p ) ) ), cons );
    auto empty = [ = ]( std::string_view in ) -> result< std::vector< T > >
    {
        return std::pair{ std::vector< T >{}, in };
    };

    return alt( nonempty, empty );
}

/*     list = '[', sep_by( number, ',' ), ']' ;                       */

auto list()
{
    return right( token( chr( '[' ) ),
                  left( sep_by( number(), token( chr( ',' ) ) ),
                        token( chr( ']' ) ) ) );
}

/* Recursive grammars need a little help, because a parser cannot
 * contain itself as a value. The usual trick is to refer to it
 * through a ‹std::function› which is filled in later. Here, we parse
 * nested lists of numbers and compute the sum of all numbers inside:
 *
 *     value = number | '[', sep_by( value, ',' ), ']' ;              */

int nested_sum( std::string_view in )
{
    std::function< result< int >( std::string_view ) > value;
    auto rec = [ & ]( std::string_view s ) { return value( s ); };

    auto sum = []( const std::vector< int > &xs )
    {
        int s = 0;
        for ( int x : xs ) s += x;
        return s;
    };

    auto nested = map( right( token( chr( '[' ) ),
                              left( sep_by( rec, token( chr( ',' ) ) ),
                                    token( chr( ']' ) ) ) ),
                       sum );

    value = alt( number(), nested );

    auto r = value( in );
    return r && r->second.empty() ? r->first : -1;
}

int main() /* demo */
{
    assert( number()( "123 rest" )->first == 123 );
    assert( number()( "123 rest" )->second == "rest" );
    assert( !number()( "x" ) );

    assert(( list()( "[1, 2, 3]" )->first == std::vector{ 1, 2, 3 } ));
    assert(( list()( "[ ]" )->first == std::vector< int >{} ));
    assert( !list()( "[1, 2," ) );
    assert( !list()( "[1 2]" ) );

    assert( nested_sum( "7" ) == 7 );
    assert( nested_sum( "[1, [2, 3], [[4]], []]" ) == 10 );
    assert( nested_sum( "[1, [2, 3]" ) == -1 );

    return 0;
}
