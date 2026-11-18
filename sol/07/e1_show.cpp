#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

/* Write a function ‹show› which prints a type (given as an
 * S-expression, see the introduction) in the conventional infix
 * notation:
 *
 *  • ‹int› and ‹bool› are printed as they are,
 *  • a function with one parameter is printed as ‹A -> R›; the arrow
 *    is right-associative, so ‹A› needs parentheses if it is itself a
 *    function type, but ‹R› never does,
 *  • a function with zero or more than one parameters is printed as
 *    ‹(A, B, …) -> R› (and ‹() -> R›); the parameters, separated by
 *    a comma and a space, never need extra parentheses.
 *
 * You can assume that the input is a well-formed type. */

// std::string show( std::string_view type );

std::string show( const sexp &t )
{
    if ( t.is_atom() )
        return t.atom;

    std::size_t params = t.items.size() - 2;
    std::string result = show( t.items.back() );

    if ( params == 1 )
    {
        const sexp &p = t.items[ 1 ];
        std::string ps = show( p );
        return ( p.is_list() ? "(" + ps + ")" : ps ) + " -> " + result;
    }

    std::string ps;
    for ( std::size_t i = 1; i + 1 < t.items.size(); ++ i )
        ps += ( i > 1 ? ", " : "" ) + show( t.items[ i ] );
    return "(" + ps + ") -> " + result;
}

std::string show( std::string_view type )
{
    return show( read( type ) );
}

int main()
{
    assert( show( "int" ) == "int" );
    assert( show( "(-> int bool)" ) == "int -> bool" );
    assert( show( "(-> int (-> int int))" ) == "int -> int -> int" );
    assert( show( "(-> (-> int int) int)" ) == "(int -> int) -> int" );
    assert( show( "(-> int bool int)" ) == "(int, bool) -> int" );
    assert( show( "(-> int)" ) == "() -> int" );
    assert( show( "(-> (-> int bool) (-> bool int) (-> int int))" ) ==
            "(int -> bool, bool -> int) -> int -> int" );
    assert( show( "(-> (-> int) bool)" ) == "(() -> int) -> bool" );

    return 0;
}
