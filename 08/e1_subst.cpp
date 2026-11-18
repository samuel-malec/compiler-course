#include <cassert>
#include <map>
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

/* Write two functions on types (in the notation of the introduction):
 *
 *  • ‹apply( s, t )› – apply the substitution ‹s› (a map from type
 *    variables to types, given as text) to the type ‹t›: replace every
 *    occurrence of a variable which is a key of ‹s› by the
 *    corresponding type; the replacement is «simultaneous» (the
 *    inserted types are not substituted again); the result is written
 *    with ‹write› (and «not» normalized),
 *  • ‹occurs( v, t )› – whether the type variable ‹v› occurs in ‹t›.
 *
 * You can assume that the inputs are well formed. */

// std::string apply( const std::map< std::string, std::string > &s, std::string_view t );
// bool occurs( std::string_view v, std::string_view t );

int main()
{
    std::map< std::string, std::string > s{ { "'a", "int" }, { "'b", "(-> 'a bool)" } };

    assert( apply( s, "'a" ) == "int" );
    assert( apply( s, "'c" ) == "'c" );
    assert( apply( s, "bool" ) == "bool" );
    assert( apply( s, "(-> 'a 'b)" ) == "(-> int (-> 'a bool))" );
    assert( apply( s, "(-> (-> 'b 'c) 'a 'a)" ) == "(-> (-> (-> 'a bool) 'c) int int)" );
    assert( apply( {}, "(-> 'x 'y)" ) == "(-> 'x 'y)" );

    assert( occurs( "'a", "'a" ) );
    assert( !occurs( "'a", "'b" ) );
    assert( occurs( "'a", "(-> int (-> bool 'a))" ) );
    assert( !occurs( "'a", "(-> int 'ab)" ) );
    assert( !occurs( "'a", "int" ) );

    return 0;
}
