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

/* A declaration «shadows» another one if it declares the same name as
 * a variable which is visible at that point (declared earlier in an
 * enclosing block). Shadowing is legal, but it is a common source of
 * bugs, and many compilers can warn about it. Write a function
 * ‹shadowing› which returns the names of all shadowing declarations
 * in a program in the core language, in program order. You can assume
 * that the program is well formed, and that no block declares the
 * same name twice. */

// std::vector< std::string > shadowing( std::string_view program );

int main()
{
    using v = std::vector< std::string >;

    assert(( shadowing( "(var x 1)" ) == v{} ));
    assert(( shadowing( "(block (var x 1) (block (var x 2)))" ) == v{ "x" } ));
    assert(( shadowing( "(block (block (var x 2)) (var x 1))" ) == v{} ));
    assert(( shadowing( "(block (var a 1) (var b 1) (block (var b 2) (block (var a 3) (var c 1))))" ) ==
             v{ "b", "a" } ));
    assert(( shadowing( "(block (var i 0) (while i (block (var i 1))))" ) == v{ "i" } ));
    assert(( shadowing( "(block (block (var x 1)) (block (var x 2)))" ) == v{} ));

    return 0;
}
