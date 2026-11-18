#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <set>

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

/* The «untyped λ-calculus» is the smallest language with first-class
 * functions: a term is either a variable ‹x›, a function ‹(fn x t)›
 * with a single parameter, or an application ‹(t u)› of one term to
 * another. Despite its size, it can express any computation, e.g.
 * using «Church numerals» – the number ‹n› is the function which
 * applies its first argument ‹n› times to its second:
 *
 *     two  = (fn f (fn x (f (f x))))
 *     plus = (fn m (fn n (fn f (fn x ((m f) ((n f) x))))))
 *
 * Computation is done by «β-reduction»: a «redex» ‹((fn x t) u)› is
 * replaced by ‹t› with ‹u› substituted for ‹x›. A term without any
 * redexes is in «normal form». Write a function ‹normalize› which
 * reduces a term to normal form using the «normal order» strategy:
 * always reduce the «leftmost-outermost» redex (the one whose ‹(›
 * comes first in the written term), including redexes inside function
 * bodies. Normal order finds the normal form whenever it exists, but
 * some terms have none (like ‹((fn x (x x)) (fn x (x x)))›), so stop
 * and return ‹std::nullopt› once more than ‹max_steps› reductions
 * would be needed.
 *
 * Substitution must avoid variable capture, exactly as in
 * ‹03/p4_subst›: when substituting ‹u› for ‹x› in ‹(fn y t)›, where ‹y›
 * is free in ‹u› and ‹x› is free in ‹t›, rename ‹y› to ‹y› followed by
 * as few ‹'› as needed so that the new name is free neither in ‹u›
 * nor in ‹t›. The result is written with ‹write›. You can assume that
 * the input is well formed. */

// std::optional< std::string > normalize( std::string_view term, int max_steps );

struct out_of_steps {};

sexp atom( std::string a ) { return sexp{ a, {}, 0 }; }
sexp fn( std::string x, sexp b ) { return sexp{ "", { atom( "fn" ), atom( x ), b }, 0 }; }
sexp app( sexp f, sexp a ) { return sexp{ "", { f, a }, 0 }; }

bool is_fn( const sexp &t ) { return t.is_list() && t.items.size() == 3 && t.items[ 0 ].atom == "fn"; }

std::set< std::string > free_vars( const sexp &t )
{
    if ( t.is_atom() ) return { t.atom };
    if ( is_fn( t ) )
    {
        auto s = free_vars( t.items[ 2 ] );
        s.erase( t.items[ 1 ].atom );
        return s;
    }
    auto s = free_vars( t.items[ 0 ] );
    s.merge( free_vars( t.items[ 1 ] ) );
    return s;
}

sexp subst( const sexp &t, const std::string &x, const sexp &u )
{
    if ( t.is_atom() )
        return t.atom == x ? u : t;
    if ( !is_fn( t ) )
        return app( subst( t.items[ 0 ], x, u ), subst( t.items[ 1 ], x, u ) );

    const std::string &y = t.items[ 1 ].atom;
    const sexp &body = t.items[ 2 ];
    if ( y == x )
        return t;

    auto fu = free_vars( u ), fb = free_vars( body );
    if ( fu.contains( y ) && fb.contains( x ) )
    {
        std::string fresh = y + "'";
        while ( fu.contains( fresh ) || fb.contains( fresh ) )
            fresh += "'";
        return fn( fresh, subst( subst( body, y, atom( fresh ) ), x, u ) );
    }

    return fn( y, subst( body, x, u ) );
}

/* One normal-order step; returns false if ‹t› is in normal form. */

bool step( sexp &t )
{
    if ( t.is_atom() )
        return false;
    if ( is_fn( t ) )
        return step( t.items[ 2 ] );
    if ( is_fn( t.items[ 0 ] ) ) /* the outermost redex is this one */
    {
        sexp r = subst( t.items[ 0 ].items[ 2 ], t.items[ 0 ].items[ 1 ].atom, t.items[ 1 ] );
        t = std::move( r );
        return true;
    }
    return step( t.items[ 0 ] ) || step( t.items[ 1 ] );
}

std::optional< std::string > normalize( std::string_view term, int max_steps )
{
    sexp t = read( term );
    for ( int i = 0; step( t ); ++ i )
        if ( i + 1 > max_steps )
            return std::nullopt;
    return write( t );
}

int main()
{
    auto n = []( std::string_view t ) { return normalize( t, 1000 ); };

    assert( n( "x" ) == "x" );
    assert( n( "((fn x x) y)" ) == "y" );
    assert( n( "(fn x ((fn y y) x))" ) == "(fn x x)" );
    assert( n( "((fn x (fn y x)) y)" ) == "(fn y' y)" );
    assert( n( "((fn x (fn y (x y))) (y z))" ) == "(fn y' ((y z) y'))" );
    assert( n( "((fn x (fn y y)) ((fn x (x x)) (fn x (x x))))" ) == "(fn y y)" );
    assert( !n( "((fn x (x x)) (fn x (x x)))" ) );
    assert( n( "(((fn x (fn y (y x))) a) b)" ) == "(b a)" );

    std::string two = "(fn f (fn x (f (f x))))",
                three = "(fn f (fn x (f (f (f x)))))",
                plus = "(fn m (fn n (fn f (fn x ((m f) ((n f) x))))))",
                mult = "(fn m (fn n (fn f (m (n f)))))";

    assert( n( "((" + plus + " " + two + ") " + two + ")" ) ==
            "(fn f (fn x (f (f (f (f x))))))" );
    assert( n( "((" + mult + " " + two + ") " + three + ")" ) ==
            "(fn f (fn x (f (f (f (f (f (f x))))))))" );

    assert( normalize( "((fn x x) ((fn x x) y))", 2 ) == "y" );
    assert( !normalize( "((fn x x) ((fn x x) y))", 1 ) );

    return 0;
}
