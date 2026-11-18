#include <cassert>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* Lexer generators (like ‹flex› or ‹re2c›) take a list of regular
 * expressions, one per token class, translate each of them into a
 * «deterministic finite automaton» (DFA), and generate a lexer which
 * runs those automata on the input. In this exercise, we will skip
 * the translation (that is a topic for another day) and write the
 * part that «runs» the automata.
 *
 * A DFA is given by the following structure. The states are
 * integers; ‹delta› is the transition function: ‹delta.at( { q, c } )›
 * is the state the automaton moves to from state ‹q› when it reads
 * the character ‹c›. If there is no entry for ‹{ q, c }›, the
 * automaton gets «stuck» and cannot read any more characters. */

struct dfa
{
    std::string name;     /* the token class; empty = skip the token */
    int start = 0;
    std::set< int > accepting = {};
    std::map< std::pair< int, char >, int > delta = {};
};

/* Write a function ‹tokenize› which splits the input into tokens
 * using a list of DFAs (one per token class), like this:
 *
 *  1. At the current position, run each DFA from its start state, as
 *     far as it can go (until it gets stuck or the input ends), and
 *     remember the «longest» prefix of the remaining input that it
 *     «accepts» (i.e. after which it was in an accepting state). Note
 *     that this is not necessarily the place where it got stuck!
 *  2. The class with the longest accepted prefix wins (maximal
 *     munch); if several classes accept a prefix of the same length,
 *     the one which comes «first» in the list wins (this is how
 *     keywords take priority over identifiers).
 *  3. If the winning class has a non-empty name, emit the pair (name,
 *     lexeme); otherwise, skip the lexeme (this is used for
 *     whitespace). Continue after the lexeme.
 *  4. If no class accepts a non-empty prefix, return ‹std::nullopt›.
 */

// std::optional< std::vector< std::pair< std::string, std::string > > >
// tokenize( const std::vector< dfa > &classes, std::string_view input );

/* Run a single automaton on (a prefix of) ‹in›, returning the length
 * of the longest accepted prefix (0 if none). */

std::size_t run( const dfa &d, std::string_view in )
{
    int state = d.start;
    std::size_t accepted = 0;

    for ( std::size_t i = 0; i < in.size(); ++ i )
    {
        auto it = d.delta.find( { state, in[ i ] } );
        if ( it == d.delta.end() )
            break;

        state = it->second;
        if ( d.accepting.contains( state ) )
            accepted = i + 1;
    }

    return accepted;
}

std::optional< std::vector< std::pair< std::string, std::string > > >
tokenize( const std::vector< dfa > &classes, std::string_view input )
{
    std::vector< std::pair< std::string, std::string > > out;

    while ( !input.empty() )
    {
        const dfa *best = nullptr;
        std::size_t best_len = 0;

        for ( const auto &d : classes )
            if ( auto len = run( d, input ); len > best_len )
                best = &d, best_len = len;

        if ( !best )
            return std::nullopt;

        if ( !best->name.empty() )
            out.emplace_back( best->name, input.substr( 0, best_len ) );

        input.remove_prefix( best_len );
    }

    return out;
}

int main()
{
    /* Add transitions ‹from → to› for all characters in ‹lo … hi›. */
    auto range = []( dfa &d, int from, char lo, char hi, int to )
    {
        for ( char c = lo; c <= hi; ++ c )
            d.delta[ { from, c } ] = to;
    };

    dfa kw_if{ "if", 0, { 2 } };
    range( kw_if, 0, 'i', 'i', 1 );
    range( kw_if, 1, 'f', 'f', 2 );

    dfa ident{ "id", 0, { 1 } };
    range( ident, 0, 'a', 'z', 1 );
    range( ident, 1, 'a', 'z', 1 );

    dfa number{ "num", 0, { 1 } };
    range( number, 0, '0', '9', 1 );
    range( number, 1, '0', '9', 1 );

    /* ‹[0-9]+ '.' [0-9]+› */
    dfa fp{ "float", 0, { 3 } };
    range( fp, 0, '0', '9', 1 );
    range( fp, 1, '0', '9', 1 );
    range( fp, 1, '.', '.', 2 );
    range( fp, 2, '0', '9', 3 );
    range( fp, 3, '0', '9', 3 );

    dfa dot{ "dot", 0, { 1 } };
    range( dot, 0, '.', '.', 1 );

    /* ‹<›, ‹<=› and ‹<<› */
    dfa op{ "op", 0, { 1, 2, 3 } };
    range( op, 0, '<', '<', 1 );
    range( op, 1, '=', '=', 2 );
    range( op, 1, '<', '<', 3 );

    dfa space{ "", 0, { 1 } };
    range( space, 0, ' ', ' ', 1 );
    range( space, 1, ' ', ' ', 1 );

    std::vector< dfa > all{ kw_if, ident, fp, number, dot, op, space };

    using v = std::vector< std::pair< std::string, std::string > >;

    assert(( tokenize( all, "" ) == v{} ));
    assert(( tokenize( all, "if" ) == v{ { "if", "if" } } ));
    assert(( tokenize( all, "iffy" ) == v{ { "id", "iffy" } } ));
    assert(( tokenize( all, "if  x" ) == v{ { "if", "if" }, { "id", "x" } } ));
    assert(( tokenize( all, "x1" ) == v{ { "id", "x" }, { "num", "1" } } ));
    assert(( tokenize( all, "a<=b<<c<" ) ==
             v{ { "id", "a" }, { "op", "<=" }, { "id", "b" },
                { "op", "<<" }, { "id", "c" }, { "op", "<" } } ));
    assert(( tokenize( all, "1.5" ) == v{ { "float", "1.5" } } ));
    assert(( tokenize( all, "12.x" ) ==
             v{ { "num", "12" }, { "dot", "." }, { "id", "x" } } ));
    assert(( tokenize( all, "1.2.3" ) ==
             v{ { "float", "1.2" }, { "dot", "." }, { "num", "3" } } ));

    assert( !tokenize( all, "A" ) );
    assert( !tokenize( all, "if x = 1" ) );

    /* Order matters: with identifiers first, ‹if› is an identifier. */
    std::vector< dfa > swapped{ ident, kw_if, space };
    assert(( tokenize( swapped, "if" ) == v{ { "id", "if" } } ));

    return 0;
}
