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

/* Write an evaluator for the language of ‹p1_letrec› (with
 * ‹letrec›), which implements «proper tail calls»: evaluating an
 * expression in tail position – a branch of ‹if›, the body of ‹let›
 * or ‹letrec›, or the body of a called function – must not use any
 * additional C++ stack space (see ‹d2_tailcall›). Consequently,
 * tail-recursive functions can run for any number of iterations.
 * Non-tail recursion (like ‹(* n (fact (- n 1)))›) may still use the
 * C++ stack, and the tests only use it to a modest depth.
 *
 * Return the integer value of the program, or ‹std::nullopt› on the
 * errors of ‹p1_letrec›. */

// std::optional< long > eval( std::string_view program );

int main()
{
    const char *loop = "(letrec ((loop (fn (n acc) (if (= n 0) acc (loop (- n 1) (+ acc 1))))))"
                       " (loop 60000 0))";
    assert( eval( loop ) == 60000 );

    const char *mutual = "(letrec ((even (fn (n) (if (= n 0) 1 (odd (- n 1)))))"
                         "         (odd (fn (n) (if (= n 0) 0 (even (- n 1))))))"
                         " (even 60001))";
    assert( eval( mutual ) == 0 );

    /* a tail call through a ‹let› body and a higher-order function */
    const char *ho = "(letrec ((apply (fn (f x) (f x)))"
                     "         (count (fn (n) (if (= n 0) 7 (let m (- n 1) (apply count m))))))"
                     " (count 30000))";
    assert( eval( ho ) == 7 );

    assert( eval( "(letrec ((f (fn (n) (if (= n 0) 0 (+ 1 (f (- n 1))))))) (f 1000))" ) == 1000 );
    assert( !eval( "(letrec ((f (fn (n) (if (= n 0) (/ 1 0) (f (- n 1)))))) (f 100))" ) );

    return 0;
}
