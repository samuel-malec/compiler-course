#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

/* Write an evaluator for the language of ‹e1_apply› which supports
 * both «lexical» and «dynamic» scoping, selected by the ‹dynamic›
 * argument. With lexical scoping, the function body is evaluated in
 * the environment where the function was «created» (extended with the
 * parameters), as usual. With dynamic scoping, it is evaluated in the
 * environment of the «call» (again extended with the parameters); a
 * function value is then just its code, there is nothing to capture.
 *
 * Return the integer value of the program, or ‹std::nullopt› on any
 * error, as in ‹e1_apply›. Look carefully at the tests – they show
 * the most important differences between the two disciplines. */

// std::optional< long > eval( std::string_view program, bool dynamic );

int main()
{
    auto both = []( std::string_view p ) { return std::pair{ eval( p, false ), eval( p, true ) }; };
    using r = std::pair< std::optional< long >, std::optional< long > >;

    /* no difference */
    assert(( both( "((fn (x y) (- x y)) 5 3)" ) == r{ 2, 2 } ));

    /* which ‹x› does ‹f› see? */
    assert(( both( "(let x 1 (let f (fn () x) (let x 2 (f))))" ) == r{ 1, 2 } ));

    /* a ‘free’ variable can be supplied by the caller */
    assert(( both( "(let f (fn () y) (let y 5 (f)))" ) == r{ std::nullopt, 5 } ));

    /* recursion without ‹letrec›: the caller's environment contains ‹f› */
    assert(( both( "(let f (fn (n) (if n (* n (f (- n 1))) 1)) (f 5))" ) == r{ std::nullopt, 120 } ));

    /* closures do not work: ‹x› is gone by the time the inner function runs */
    assert(( both( "(let add (fn (x) (fn (y) (+ x y))) ((add 1) 2))" ) == r{ 3, std::nullopt } ));

    /* … unless the caller happens to have an ‹x› of its own */
    assert(( both( "(let add (fn (x) (fn (y) (+ x y))) (let x 100 ((add 1) 2)))" ) == r{ 3, 102 } ));

    /* parameters shadow the caller's variables in both cases */
    assert(( both( "(let x 1 (let f (fn (x) x) (f 7)))" ) == r{ 7, 7 } ));

    return 0;
}
