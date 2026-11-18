#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <map>

/* The programs in this exercise are written as S-expressions, so
 * that we can concentrate on evaluation instead of parsing. The
 * function ‹read› (provided) turns the text into a tree of ‹sexp›
 * nodes: each node is either an «atom» – a number, a name, a string
 * literal «including» its quotes (no escapes), or anything else
 * without spaces and parentheses – or a «list» of nodes. ‹offset›
 * is the position of the first character of the node in the input.
 * The function ‹write› does the opposite of ‹read›. If the input is
 * malformed, ‹read› throws ‹std::invalid_argument›. */

struct sexp
{
    std::string atom;           /* non-empty for atoms */
    std::vector< sexp > items;  /* elements of a list */
    std::size_t offset = 0;

    bool is_atom() const { return !atom.empty(); }
    bool is_list() const { return atom.empty(); }
};

void skip_space( std::string_view s, std::size_t &pos )
{
    while ( pos < s.size() && ( s[ pos ] == ' ' || s[ pos ] == '\n' ) )
        ++ pos;
}

sexp read_at( std::string_view s, std::size_t &pos )
{
    skip_space( s, pos );
    sexp out{ "", {}, pos };

    if ( pos >= s.size() || s[ pos ] == ')' )
        throw std::invalid_argument( "unexpected end of list or input" );

    if ( s[ pos ] == '(' )
    {
        for ( ++ pos; skip_space( s, pos ), pos >= s.size() || s[ pos ] != ')'; )
            out.items.push_back( read_at( s, pos ) );
        ++ pos;
        return out;
    }

    std::size_t start = pos;

    if ( s[ pos ] == '"' )
    {
        pos = s.find( '"', pos + 1 );
        if ( pos == s.npos )
            throw std::invalid_argument( "unterminated string" );
        ++ pos;
    }
    else
        while ( pos < s.size() && std::string_view( " \n()" ).find( s[ pos ] ) == s.npos )
            ++ pos;

    out.atom = s.substr( start, pos - start );
    return out;
}

sexp read( std::string_view s )
{
    std::size_t pos = 0;
    sexp e = read_at( s, pos );
    skip_space( s, pos );
    if ( pos != s.size() )
        throw std::invalid_argument( "trailing input" );
    return e;
}

std::string write( const sexp &e )
{
    if ( e.is_atom() )
        return e.atom;

    std::string out;
    for ( const auto &i : e.items )
        out += ( out.empty() ? "" : " " ) + write( i );
    return "(" + out + ")";
}

/* A spreadsheet is a nice example of an interpreter where the order
 * of evaluation is not given by the program text, but by the
 * «dependencies» between the formulas. Each cell has a name and a
 * formula (an S-expression):
 *
 *  • an integer literal (digits with an optional leading ‹-›),
 *  • a reference to another cell – any other atom, e.g. ‹A1›,
 *  • ‹(+ e …)› and ‹(* e …)› – the sum and product of one or more
 *    operands, ‹(- a b)›, ‹(/ a b)› (truncating), ‹(max e …)› and
 *    ‹(min e …)› (one or more operands).
 *
 * Write a function ‹compute› which takes the formulas of all cells
 * (as a map from cell names to formula texts) and returns the values
 * of all cells. The value of a cell is ‹std::nullopt› if:
 *
 *  • its formula is malformed (cannot be read, unknown operator,
 *    wrong number of operands),
 *  • it refers to a cell which does not exist,
 *  • it divides by zero,
 *  • it is a part of a «cycle» of references (a cell which refers,
 *    directly or indirectly, to itself),
 *  • or it refers to a cell whose value is ‹std::nullopt›.
 *
 * Each cell must be evaluated at most once: when a cell is needed by
 * several other cells, remember its value. (The tests contain a
 * spreadsheet where evaluating cells repeatedly would take
 * exponential time.) To detect cycles, keep track of the cells which
 * are «being evaluated» right now: if you need the value of such a
 * cell, you have found a cycle. */

using sheet = std::map< std::string, std::string >;
using values = std::map< std::string, std::optional< long > >;

// values compute( const sheet &cells );

int main()
{
    sheet s =
    {
        { "A1", "10" },
        { "A2", "(+ A1 1)" },
        { "A3", "(* A1 A2 2)" },
        { "B1", "(max A1 A2 -5)" },
        { "B2", "(- (min A3 B1) 100)" },
        { "B3", "(/ A3 (- A1 10))" },
        { "C1", "(+ C2 1)" },
        { "C2", "(+ C1 1)" },
        { "C3", "(+ C2 A1)" },
        { "D1", "(+ X9 1)" },
        { "D2", "(foo 1)" },
        { "D3", "(- 1)" },
        { "D4", "(+ D1 A1)" },
        { "D5", "(+ 1" },
        { "E1", "E1" },
        { "E2", "(/ -7 2)" },
    };

    values v = compute( s );
    assert( v.size() == s.size() );
    assert( v[ "A1" ] == 10 );
    assert( v[ "A2" ] == 11 );
    assert( v[ "A3" ] == 220 );
    assert( v[ "B1" ] == 11 );
    assert( v[ "B2" ] == -89 );
    assert( !v[ "B3" ] );
    assert( !v[ "C1" ] );
    assert( !v[ "C2" ] );
    assert( !v[ "C3" ] );
    assert( !v[ "D1" ] );
    assert( !v[ "D2" ] );
    assert( !v[ "D3" ] );
    assert( !v[ "D4" ] );
    assert( !v[ "D5" ] );
    assert( !v[ "E1" ] );
    assert( v[ "E2" ] == -3 );

    /* A long chain of dependencies, and a ‘diamond’ which doubles the
     * number of paths at each step. */
    sheet big{ { "X0", "1" }, { "Y0", "1" } };
    for ( int i = 1; i <= 2000; ++ i )
        big[ "X" + std::to_string( i ) ] = "(+ X" + std::to_string( i - 1 ) + " 1)";
    for ( int i = 1; i <= 60; ++ i )
    {
        auto prev = "Y" + std::to_string( i - 1 );
        big[ "Y" + std::to_string( i ) ] = "(- (+ " + prev + " " + prev + ") " + prev + ")";
    }

    values w = compute( big );
    assert( w[ "X2000" ] == 2001 );
    assert( w[ "Y60" ] == 1 );

    return 0;
}
