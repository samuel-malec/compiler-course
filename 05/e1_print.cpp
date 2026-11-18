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

/* Write a function ‹run› which executes a program written in the core
 * language from the introduction, «without» ‹if› and ‹while›:
 *
 *     stmt = (block stmt …) | (var name expr) | (set name expr)
 *          | (print expr) ;
 *     expr = integer | name | ( op expr expr ) ;
 *
 * where ‹op› is one of ‹+ - * / % < <= > >= = !=› (comparisons give 1
 * or 0; ‹/› and ‹%› as in C++). A program is a single statement. The
 * result is the list of printed values. Return ‹std::nullopt› if the
 * program is malformed, uses an undeclared variable, declares the same
 * variable twice in one block, or divides by zero. Variables declared
 * in a block are only visible until the end of that block. */

// std::optional< std::vector< long > > run( std::string_view program );

int main()
{
    using v = std::vector< long >;

    assert(( run( "(print 7)" ) == v{ 7 } ));
    assert(( run( "(block (var x 2) (set x (* x 21)) (print x) (print (< x 50)))" ) ==
             v{ 42, 1 } ));
    assert(( run( "(block (var x 1) (block (var x 2) (print x)) (print x))" ) == v{ 2, 1 } ));
    assert(( run( "(block (var x 1) (block (set x 2)) (print x))" ) == v{ 2 } ));
    assert(( run( "(block (var x 7) (block (var x (+ x 1)) (print x)))" ) == v{ 8 } ));
    assert(( run( "(block (print (% -7 3)) (print (!= 1 2)) (print (>= 2 2)))" ) ==
             v{ -1, 1, 1 } ));
    assert(( run( "(block)" ) == v{} ));

    assert( !run( "(print x)" ) );
    assert( !run( "(block (block (var x 1)) (print x))" ) );
    assert( !run( "(block (var x 1) (var x 2))" ) );
    assert( !run( "(set x 1)" ) );
    assert( !run( "(print (/ 1 0))" ) );
    assert( !run( "(print 1 2)" ) );
    assert( !run( "(frobnicate)" ) );

    return 0;
}
