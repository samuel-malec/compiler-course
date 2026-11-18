#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <utility>

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

/* Recursive data – lists, trees, streams – needs «recursive types».
 * A recursive type is written ‹(mu t T)›, where the type variable ‹t›
 * stands for the whole type inside ‹T›: ‹(mu t (-> int t))› is the type
 * of functions which take an ‹int› and return another such function.
 * In the «equi-recursive» view, ‹(mu t T)› is «equal» to its
 * «unfolding» – ‹T› with ‹(mu t T)› substituted for ‹t› – so it stands
 * for an infinite type tree:
 *
 *     (mu t (-> int t))  =  (-> int (mu t (-> int t)))
 *                        =  (-> int (-> int (-> int …)))
 *
 * Two types are equal if their infinite trees are equal. Write a
 * function ‹equal› which decides this, for the types:
 *
 *     type = 'int' | 'bool' | var | (-> type type) | (mu var type) ;
 *
 * You can assume that the types are «closed» (every variable is bound
 * by an enclosing ‹mu›), and «contractive» (the body of a ‹mu› is never
 * just a variable or another ‹mu›).
 *
 * Comparing infinite trees by unfolding forever does not terminate.
 * The standard algorithm keeps a set of «assumptions» – pairs of types
 * which are being compared right now: when the same pair comes up
 * again, it can be assumed equal (this is reasoning by «coinduction»).
 * Since a type has only finitely many distinct sub-terms (up to
 * unfolding), this terminates. */

// bool equal( std::string_view a, std::string_view b );

using type = sexp;

bool is_mu( const type &t ) { return t.is_list() && t.items[ 0 ].atom == "mu"; }

type subst( const type &t, const std::string &v, const type &r )
{
    if ( t.is_atom() ) return t.atom == v ? r : t;
    if ( is_mu( t ) && t.items[ 1 ].atom == v ) return t; /* shadowed */
    type out = t;
    for ( std::size_t i = 1; i < out.items.size(); ++ i )
        out.items[ i ] = subst( t.items[ i ], v, r );
    return out;
}

type unfold( const type &mu ) { return subst( mu.items[ 2 ], mu.items[ 1 ].atom, mu ); }

using assumptions = std::set< std::pair< std::string, std::string > >;

bool equal( const type &a, const type &b, assumptions &seen )
{
    auto key = std::pair{ write( a ), write( b ) };
    if ( seen.contains( key ) )
        return true; /* coinductive hypothesis */

    if ( is_mu( a ) || is_mu( b ) )
    {
        seen.insert( key );
        return equal( is_mu( a ) ? unfold( a ) : a, is_mu( b ) ? unfold( b ) : b, seen );
    }

    if ( a.is_atom() || b.is_atom() )
        return a.is_atom() && b.is_atom() && a.atom == b.atom;

    seen.insert( key );
    return a.items.size() == b.items.size() &&
           equal( a.items[ 1 ], b.items[ 1 ], seen ) && equal( a.items[ 2 ], b.items[ 2 ], seen );
}

bool equal( std::string_view a, std::string_view b )
{
    assumptions seen;
    return equal( read( a ), read( b ), seen );
}

int main()
{
    assert( equal( "int", "int" ) );
    assert( !equal( "int", "bool" ) );
    assert( equal( "(-> int bool)", "(-> int bool)" ) );
    assert( equal( "(mu t (-> int t))", "(-> int (mu t (-> int t)))" ) );
    assert( equal( "(mu t (-> int t))", "(mu s (-> int (-> int s)))" ) );
    assert( !equal( "(mu t (-> int t))", "(mu t (-> bool t))" ) );
    assert( equal( "(mu t (-> t int))", "(-> (mu t (-> t int)) int)" ) );
    assert( equal( "(mu a (-> a a))", "(mu b (-> (-> b b) (-> b b)))" ) );
    assert( !equal( "(mu a (-> a int))", "(mu a (-> int a))" ) );
    assert( equal( "(mu x (-> int (mu y (-> x y))))", "(mu x (-> int (mu y (-> x y))))" ) );
    assert( !equal( "(mu t (-> int t))", "(-> int (-> int int))" ) );
    assert( equal( "(-> (mu t (-> bool t)) int)", "(-> (-> bool (mu u (-> bool u))) int)" ) );

    return 0;
}
