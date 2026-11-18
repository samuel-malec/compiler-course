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

/* Extend ‹p1_mono› with let-polymorphism – that is, implement
 * Algorithm W (see ‹d2_infer›): the type of a ‹let›-bound expression
 * is generalized over the type variables which do not occur in the
 * context, and every use of the variable instantiates the resulting
 * scheme with fresh type variables. Function parameters stay
 * monomorphic. The interface is the same as in ‹p1_mono›. */

// std::optional< std::string > infer( std::string_view expr );

int main()
{
    assert( infer( "(let id (fn (x) x) (if (id #t) (id 1) 0))" ) == "int" );
    assert( infer( "(let id (fn (x) x) (id id))" ) == "(-> 'a 'a)" );
    assert( infer( "(let k (fn (x y) x) (k (k 1 #t) (fn (z) z)))" ) == "int" );
    assert( infer( "(let twice (fn (f x) (f (f x))) (if (twice (fn (b) (not b)) #t) (twice (fn (n) (+ n 1)) 0) 1))" ) ==
            "int" );
    assert( infer( "(let compose (fn (f) (fn (g) (fn (x) (f (g x))))) ((compose compose) compose))" ) ==
            "(-> (-> 'a 'b) (-> (-> 'c (-> 'd 'a)) (-> 'c (-> 'd 'b))))" );
    assert( infer( "(fn (y) (let f (fn (x) y) (f 1)))" ) == "(-> 'a 'a)" );
    assert( infer( "(fn (y) (let f (fn (x) (+ x y)) (f 1)))" ) == "(-> int int)" );
    assert( infer( "(let pick (fn (b) (fn (x y) (if b x y))) (let p (pick #t) (p 1 2)))" ) == "int" );

    /* the context variable ‹'y› must not be generalized */
    assert( !infer( "(fn (y) (let f (fn (x) y) (if (f 1) (+ (f 1) 1) 0)))" ) );
    /* parameters are monomorphic */
    assert( !infer( "(fn (id) (if (id #t) (id 1) 0))" ) );
    assert( !infer( "(let f (fn (x) (x x)) 1)" ) );

    return 0;
}
