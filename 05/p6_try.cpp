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

/* Add «exceptions» to the core language (including ‹while›), together
 * with unlabelled ‹(break)› and ‹(continue)›:
 *
 *  • ‹(throw e)› – throw the value of ‹e› (an integer),
 *  • ‹(try s (catch x h))› – execute ‹s›; if it throws a value ‹v›,
 *    execute ‹h› in a new scope where ‹x› is declared with value ‹v›,
 *  • ‹(try s (finally f))› – execute ‹s› and then, «no matter how ‹s›
 *    completed» (normally, by throwing, or by ‹break› or ‹continue›),
 *    execute ‹f›; afterwards, continue the way ‹s› completed (e.g.
 *    keep propagating the exception) – «unless» ‹f› itself completes
 *    abnormally, in which case that completion replaces the original
 *    one,
 *  • ‹(try s (catch x h) (finally f))› – both: the ‹finally› part runs
 *    after ‹s› and (if it ran) ‹h›.
 *
 * An exception propagates out of blocks, ‹if›s and loops until it is
 * caught; one which reaches the top of the program stops it, and is
 * reported in the result as ‹uncaught›. Before running, check that
 * every ‹break› and ‹continue› is inside a loop (in the same sense as
 * in ‹p1_loops›), and that every ‹try› has the shape described above.
 * Return ‹std::nullopt› for these errors, for malformed programs and
 * for the run-time errors of ‹e1_print› (which are «not» exceptions
 * in this language, and cannot be caught). */

struct outcome
{
    std::vector< long > output;
    std::optional< long > uncaught;
};

// std::optional< outcome > run( std::string_view program );

int main()
{
    using v = std::vector< long >;
    auto out = []( std::string_view p ) { return run( p )->output; };

    assert(( out( "(try (throw 5) (catch e (print e)))" ) == v{ 5 } ));
    assert(( out( "(block (try (block (print 1) (throw 2) (print 3))"
                  " (catch e (print (* e 10)))) (print 4))" ) == v{ 1, 20, 4 } ));
    assert(( out( "(try (try (throw 1) (catch e (throw (+ e 1)))) (catch e (print e)))" ) == v{ 2 } ));
    assert(( out( "(try (try (throw 1) (finally (print 9))) (catch e (print e)))" ) == v{ 9, 1 } ));
    assert(( out( "(try (print 1) (catch e (print e)) (finally (print 2)))" ) == v{ 1, 2 } ));
    assert(( out( "(try (throw 3) (catch e (print e)) (finally (print 2)))" ) == v{ 3, 2 } ));
    assert(( out( "(try (try (throw 1) (finally (throw 2))) (catch e (print e)))" ) == v{ 2 } ));

    assert(( out( "(block (var i 0) (while 1 (block (set i (+ i 1))"
                  " (try (if (= i 3) (break)) (finally (print i))))) (print 100))" ) ==
             v{ 1, 2, 3, 100 } ));
    assert(( out( "(block (var i 0) (while (< i 3) (block (set i (+ i 1))"
                  " (try (block (continue) (print 0)) (finally (print i))))))" ) ==
             v{ 1, 2, 3 } ));
    assert(( out( "(while 1 (try (throw 1) (finally (break))))" ) == v{} ));
    assert(( out( "(block (var i 0) (while (< i 9) (block (set i (+ i 1))"
                  " (try (if (= i 2) (throw i)) (catch x (break))))) (print i))" ) == v{ 2 } ));

    auto r = run( "(block (print 1) (throw 42) (print 2))" );
    assert( r && ( r->output == v{ 1 } ) && r->uncaught == 42 );
    assert( !run( "(print 1)" )->uncaught );

    assert( !run( "(block (try (throw 1) (catch e (print e))) (print e))" ) );
    assert( !run( "(try (throw (/ 1 0)) (catch e (print e)))" ) );
    assert( !run( "(break)" ) );
    assert( !run( "(try (print 1))" ) );
    assert( !run( "(try (print 1) (finally (print 2)) (catch e (print 3)))" ) );

    return 0;
}
