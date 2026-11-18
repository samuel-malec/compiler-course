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

/* Write a «resolver» (see ‹d2_resolve›) for the full core language.
 * The program runs in an implicit «global» scope, and every ‹block›
 * opens a new scope; in addition, the body of an ‹if› or ‹while›
 * («each» of the two branches of ‹if›) is always executed in a scope
 * of its own, even if it is not a ‹block› – so ‹(if c (var x 1))›
 * declares an ‹x› which disappears right away.
 *
 * For each variable «occurrence» – an atom which is a name, in an
 * expression or as the target of ‹set› – in program order, produce a
 * string ‹NAME:DEPTH:SLOT›, where ‹DEPTH› is the number of scopes
 * between the occurrence and the scope which declares the variable
 * (0 if it is the innermost one), and ‹SLOT› is the position of the
 * declaration in its scope (the first declaration in a scope has
 * slot 0).
 *
 * Return ‹std::nullopt› if a variable is not declared, or if it is
 * declared twice in the same scope. As in ‹d2_resolve›, the
 * initializer of ‹(var x e)› is resolved «before» ‹x› is declared.
 * You can assume that the program is well formed. */

// std::optional< std::vector< std::string > > resolve( std::string_view program );

int main()
{
    using v = std::vector< std::string >;

    assert(( resolve( "(print 1)" ) == v{} ));
    assert(( resolve( "(var x 1)" ) == v{} ));
    assert(( resolve( "(block (var a 1) (var b 2) (print (+ a b)))" ) ==
             v{ "a:0:0", "b:0:1" } ));
    assert(( resolve( "(block (var a 1) (block (var b a) (set a b)))" ) ==
             v{ "a:1:0", "a:1:0", "b:0:0" } ));
    assert(( resolve( "(block (var x 1) (block (var x (+ x 1)) (print x)))" ) ==
             v{ "x:1:0", "x:0:0" } ));
    assert(( resolve( "(block (var g 0) (while (< g 3) (set g (+ g 1))))" ) ==
             v{ "g:0:0", "g:1:0", "g:1:0" } ));
    assert(( resolve( "(block (var g 0) (if g (block (var t 1) (print (+ t g))) (print g)))" ) ==
             v{ "g:0:0", "t:0:0", "g:2:0", "g:1:0" } ));

    assert( !resolve( "(print x)" ) );
    assert( !resolve( "(block (var x 1) (var x 2))" ) );
    assert( !resolve( "(block (if 1 (var t 1)) (print t))" ) );
    assert( !resolve( "(block (var x x))" ) );

    return 0;
}
