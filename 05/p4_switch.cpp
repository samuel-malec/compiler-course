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

/* Extend the core language (including ‹while›) with ‹break›,
 * ‹continue› (unlabelled, as in ‹p1_loops›) and a C-style ‹switch›:
 *
 *     (switch e (case n s …) … (default s …) …)
 *
 * The cases are written in order; each starts with an integer literal
 * (‹n›) or the word ‹default› (which may appear anywhere in the list,
 * but at most once), followed by zero or more statements. The value of
 * ‹e› selects the first case with that value, or the ‹default› case if
 * there is no such case, or nothing at all if there is no ‹default›.
 * Execution starts with the statements of the selected case and then
 * «falls through» to the statements of the following cases, until the
 * end of the switch or a ‹break›. A ‹break› inside a ‹switch› leaves
 * the innermost enclosing ‹switch› or ‹while›; a ‹continue› refers to
 * the innermost enclosing ‹while› (it passes through any switches in
 * between). The statements of each case execute in a new scope of
 * their own.
 *
 * Check the program before running it: it is an error to ‹break›
 * outside of a loop or switch, to ‹continue› outside of a loop, to
 * have two cases with the same value or two ‹default› cases. Return
 * ‹std::nullopt› for these errors, for malformed programs and for the
 * run-time errors of ‹e1_print›. */

// std::optional< std::vector< long > > run( std::string_view program );

int main()
{
    using v = std::vector< long >;

    auto sw = []( int x )
    {
        return "(switch " + std::to_string( x ) +
               " (case 1 (print 1)) (case 2 (print 2)) (case 3 (print 3) (break))"
               " (default (print 0)) (case 4 (print 4)))";
    };

    assert(( run( sw( 1 ) ) == v{ 1, 2, 3 } ));
    assert(( run( sw( 3 ) ) == v{ 3 } ));
    assert(( run( sw( 4 ) ) == v{ 4 } ));
    assert(( run( sw( 7 ) ) == v{ 0, 4 } ));
    assert(( run( "(switch 7 (case 1 (print 1)))" ) == v{} ));
    assert(( run( "(switch 1 (case 1) (case 2 (print 2)))" ) == v{ 2 } ));

    /* ‹continue› passes through the switch, ‹break› does not */
    auto loop = "(block (var i 0) (while (< i 5) (block (set i (+ i 1))"
                " (switch (% i 3) (case 0 (continue)) (case 1 (print i) (break))"
                "  (default (print (* 10 i)))) (print 100))))";
    assert(( run( loop ) == v{ 1, 100, 20, 100, 4, 100, 50, 100 } ));

    assert(( run( "(block (var x 5) (switch x (case 5 (var y 1) (print y)) (case 6 (var y 2) (print y))))" ) ==
             v{ 1, 2 } ));

    assert( !run( "(break)" ) );
    assert( !run( "(switch 1 (case 1 (continue)))" ) );
    assert( !run( "(switch 1 (case 1) (case 1))" ) );
    assert( !run( "(switch 1 (default) (default))" ) );
    assert( !run( "(switch 1 (case x))" ) );
    assert( !run( "(switch y (case 1))" ) );

    return 0;
}
