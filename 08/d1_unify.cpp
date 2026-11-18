#include <cassert>
#include <map>
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

/* This demo implements the two basic operations of type inference:
 * applying a «substitution» to a type, and «unifying» two types. As
 * in Chapter 7, types are S-expressions; type variables are atoms
 * starting with an apostrophe. */

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
bool is_var( const type &t ) { return t.is_atom() && t.atom[ 0 ] == '\''; }

/* A substitution maps type variables to types. We keep it in
 * «triangular» form: the type a variable maps to may itself mention
 * variables which are mapped further. This is cheap to extend (just
 * add one binding), and ‹apply› follows the chains. As long as we
 * never bind a variable which is already bound, and never create a
 * cycle (the occurs check takes care of that), ‹apply› terminates. */

using substitution = std::map< std::string, type >;

type apply( const substitution &s, const type &t )
{
    if ( is_var( t ) )
    {
        auto it = s.find( t.atom );
        return it == s.end() ? t : apply( s, it->second );
    }
    if ( t.is_atom() )
        return t;
    type out = t;
    for ( auto &x : out.items )
        x = apply( s, x );
    return out;
}

/* The occurs check: does the variable ‹v› occur in ‹t› (after applying
 * the substitution)? */

bool occurs( const substitution &s, const std::string &v, const type &t )
{
    type u = apply( s, t );
    if ( is_var( u ) )
        return u.atom == v;
    for ( const auto &x : u.items )
        if ( occurs( s, v, x ) )
            return true;
    return false;
}

struct unify_error : std::runtime_error { using std::runtime_error::runtime_error; };

/* Unification extends the substitution ‹s› so that ‹a› and ‹b› become
 * equal. Note how the two types are first «resolved» – if either is a
 * bound variable, we look at what it is bound to. */

void unify( substitution &s, const type &a0, const type &b0 )
{
    type a = apply( s, a0 ), b = apply( s, b0 );

    if ( is_var( a ) && is_var( b ) && a.atom == b.atom )
        return;                                     /* 'a = 'a */

    if ( is_var( a ) || is_var( b ) )
    {
        const type &v = is_var( a ) ? a : b, &t = is_var( a ) ? b : a;
        if ( occurs( s, v.atom, t ) )
            throw unify_error( "infinite type: " + v.atom + " = " + write( t ) );
        s.emplace( v.atom, t );                     /* 'a ↦ τ */
        return;
    }

    if ( a.is_atom() || b.is_atom() )
    {
        if ( a.is_atom() && b.is_atom() && a.atom == b.atom )
            return;                                 /* int = int */
        throw unify_error( "cannot unify " + write( a ) + " with " + write( b ) );
    }

    if ( a.items.size() != b.items.size() || a.items[ 0 ].atom != b.items[ 0 ].atom )
        throw unify_error( "cannot unify " + write( a ) + " with " + write( b ) );

    /* Unify the components pairwise. Each step works with the
     * substitution extended by the previous steps – ‹unify› applies it
     * at the start. */
    for ( std::size_t i = 1; i < a.items.size(); ++ i )
        unify( s, a.items[ i ], b.items[ i ] );
}

/* For tests: unify, then show both types after the substitution. */

std::string unified( std::string_view a, std::string_view b )
{
    substitution s;
    type x = read( a ), y = read( b );
    try { unify( s, x, y ); }
    catch ( const unify_error &e ) { return e.what(); }
    assert( write( apply( s, x ) ) == write( apply( s, y ) ) );
    return write( apply( s, x ) );
}

int main() /* demo */
{
    substitution s{ { "'a", read( "(-> 'b int)" ) }, { "'b", atom( "bool" ) } };
    assert( write( apply( s, read( "(-> 'a 'c)" ) ) ) == "(-> (-> bool int) 'c)" );

    assert( unified( "'a", "int" ) == "int" );
    assert( unified( "(-> 'a int)", "(-> bool 'b)" ) == "(-> bool int)" );
    assert( unified( "(-> 'a 'a)", "(-> int 'b)" ) == "(-> int int)" );
    assert( unified( "(-> 'a 'b)", "(-> 'b 'c)" ) == "(-> 'c 'c)" );   /* most general */
    assert( unified( "(-> 'a (-> 'a int))", "(-> (-> int 'b) 'c)" ) == "(-> (-> int 'b) (-> (-> int 'b) int))" );

    assert( unified( "int", "bool" ) == "cannot unify int with bool" );
    assert( unified( "(-> 'a 'a)", "(-> int bool)" ) == "cannot unify int with bool" );
    assert( unified( "(-> int int)", "(-> int int int)" ) == "cannot unify (-> int int) with (-> int int int)" );
    assert( unified( "'a", "(-> 'a int)" ) == "infinite type: 'a = (-> 'a int)" );
    assert( unified( "(-> 'a 'b)", "(-> 'b (-> 'a int))" ) == "infinite type: 'b = (-> 'b int)" );

    return 0;
}
