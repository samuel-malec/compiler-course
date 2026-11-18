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

/* Write an interpreter for the full core language from the
 * introduction (including ‹if› and ‹while›), extended with these
 * statements:
 *
 *  • ‹(break)› – leave the innermost enclosing loop,
 *  • ‹(continue)› – skip the rest of the body of the innermost loop and
 *    go on with the next iteration (evaluating the condition again),
 *  • ‹(label L (while c s))› – a loop with a name, ‹L›,
 *  • ‹(break L)› and ‹(continue L)› – the same as above, but for the
 *    enclosing loop named ‹L›, which may be several loops further out.
 *
 * Before running the program, check it: it is an error if ‹break› or
 * ‹continue› appears outside of any loop, if a labelled ‹break› or
 * ‹continue› refers to a label which is not on an enclosing loop, if
 * ‹label› is applied to anything other than ‹while›, or if a labelled
 * loop is nested in another loop with the same label. Return
 * ‹std::nullopt› for these errors, for malformed programs and for the
 * run-time errors of ‹e1_print›; otherwise return the printed values.
 *
 * Hint: let ‹exec› return a «completion» (as in ‹d1_stmt›), which, for
 * ‹break› and ‹continue›, also carries the label (if any). */

// std::optional< std::vector< long > > run( std::string_view program );

int main()
{
    using v = std::vector< long >;

    assert(( run( "(block (var i 0) (while (< i 10) (block (set i (+ i 1))"
                  "  (if (= (% i 2) 0) (continue)) (if (> i 7) (break)) (print i))))" ) ==
             v{ 1, 3, 5, 7 } ));

    auto pairs = "(block (var i 0)"
                 " (label outer (while (< i 3) (block (var j 0) (set i (+ i 1))"
                 "   (while 1 (block (set j (+ j 1))"
                 "     (if (= j 2) (continue outer))"
                 "     (if (= i 3) (break outer))"
                 "     (print (+ (* 10 i) j))))))))";
    assert(( run( pairs ) == v{ 11, 21 } ));

    assert(( run( "(block (var n 0) (label a (while (< n 3) (block (set n (+ n 1))"
                  " (label b (while 1 (break b))) (print n)))))" ) == v{ 1, 2, 3 } ));
    assert(( run( "(if 0 (print 1) (print 2))" ) == v{ 2 } ));

    assert( !run( "(break)" ) );
    assert( !run( "(block (if 1 (continue)))" ) );
    assert( !run( "(while 0 (break nope))" ) );
    assert( !run( "(label x (print 1))" ) );
    assert( !run( "(label x (while 0 (label x (while 0 (break)))))" ) );
    assert( !run( "(block (label x (while 0 (break))) (while 0 (break x)))" ) );
    assert( !run( "(while 1 (print (/ 1 0)))" ) );

    return 0;
}
