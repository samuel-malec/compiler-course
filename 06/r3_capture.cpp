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

/* A variable which is never referenced by a closure can live in a
 * stack frame (or a register), and disappear when the function
 * returns. A variable which «is» referenced by some closure may have
 * to outlive the call, so it must be allocated on the heap. Compilers
 * find out which is which by a simple static analysis.
 *
 * In the language of ‹e1_apply›, the «binders» are the parameters of
 * functions and the variables bound by ‹let›. A binder is «captured»
 * if some reference to it occurs inside a ‹fn› which is itself inside
 * the scope of the binder (i.e. inside the body of the ‹let›, or of
 * the function whose parameter it is) – the reference then crosses a
 * function boundary. Write a function ‹captured› which returns the
 * names of all captured binders, in «pre-order» of the binders (the
 * parameters of a function in the order they are listed, a ‹let›
 * binder at the position of the ‹let›). You can assume that the input
 * is well formed. */

// std::vector< std::string > captured( std::string_view program );

int main()
{
    using v = std::vector< std::string >;

    assert(( captured( "(let x 1 (+ x 1))" ) == v{} ));
    assert(( captured( "(let x 1 (let y 2 (fn (z) (+ x z))))" ) == v{ "x" } ));
    assert(( captured( "(let x 1 (fn (x) x))" ) == v{} ));
    assert(( captured( "(fn (a b) (fn () a))" ) == v{ "a" } ));
    assert(( captured( "(fn (n) (let m n (fn () m)))" ) == v{ "m" } ));
    assert(( captured( "(fn (a b c) (+ ((fn () c) ) ((fn (d) (fn () (+ a d))) 1)))" ) ==
             v{ "a", "c", "d" } ));
    assert(( captured( "(let x 1 (let f (fn () 2) (f x)))" ) == v{} ));
    assert(( captured( "(let x (fn () x) x)" ) == v{} ));
    assert(( captured( "(fn (x) (let x 1 (fn () x)))" ) == v{ "x" } ));

    return 0;
}
