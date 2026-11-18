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

/* Write a function ‹unify› which computes the most general unifier of
 * two types (see ‹d1_unify›) and returns the unified type – the result
 * of applying the unifier to either of them – in «normal form» (see
 * the introduction). Return ‹std::nullopt› if the types cannot be
 * unified. Both types may contain the same variables: ‹'a› in the first
 * type is the same variable as ‹'a› in the second. */

// std::optional< std::string > unify( std::string_view a, std::string_view b );

struct no_unifier {};

using substitution = std::map< std::string, sexp >;
bool is_var( const sexp &t ) { return t.is_atom() && t.atom[ 0 ] == '\''; }

sexp apply( const substitution &s, const sexp &t )
{
    if ( is_var( t ) )
    {
        auto it = s.find( t.atom );
        return it == s.end() ? t : apply( s, it->second );
    }
    sexp out = t;
    for ( auto &x : out.items ) x = apply( s, x );
    return out;
}

bool occurs( const std::string &v, const sexp &t )
{
    if ( t.is_atom() ) return t.atom == v;
    for ( const auto &x : t.items ) if ( occurs( v, x ) ) return true;
    return false;
}

void unify( substitution &s, const sexp &a0, const sexp &b0 )
{
    sexp a = apply( s, a0 ), b = apply( s, b0 );
    if ( is_var( a ) && is_var( b ) && a.atom == b.atom ) return;
    if ( is_var( a ) || is_var( b ) )
    {
        const sexp &v = is_var( a ) ? a : b, &t = is_var( a ) ? b : a;
        if ( occurs( v.atom, t ) ) throw no_unifier();
        s.emplace( v.atom, t );
        return;
    }
    if ( a.is_atom() || b.is_atom() )
    {
        if ( a.is_atom() && b.is_atom() && a.atom == b.atom ) return;
        throw no_unifier();
    }
    if ( a.items.size() != b.items.size() ) throw no_unifier();
    for ( std::size_t i = 0; i < a.items.size(); ++ i )
        unify( s, a.items[ i ], b.items[ i ] );
}

sexp normalize( const sexp &t, std::map< std::string, std::string > &names )
{
    if ( is_var( t ) )
    {
        if ( !names.contains( t.atom ) )
            names[ t.atom ] = std::string( "'" ) + char( 'a' + names.size() );
        return sexp{ names[ t.atom ], {}, 0 };
    }
    sexp out = t;
    for ( auto &x : out.items ) x = normalize( x, names );
    return out;
}

std::optional< std::string > unify( std::string_view a, std::string_view b )
{
    substitution s;
    sexp x = read( a ), y = read( b );
    try { unify( s, x, y ); }
    catch ( const no_unifier & ) { return std::nullopt; }
    std::map< std::string, std::string > names;
    return write( normalize( apply( s, x ), names ) );
}

int main()
{
    assert( unify( "int", "int" ) == "int" );
    assert( unify( "'x", "bool" ) == "bool" );
    assert( unify( "'x", "'y" ) == "'a" );
    assert( unify( "(-> 'x int)", "(-> bool 'y)" ) == "(-> bool int)" );
    assert( unify( "(-> 'x 'y)", "(-> 'y 'z)" ) == "(-> 'a 'a)" );
    assert( unify( "(-> 'p 'q 'p)", "(-> 'q (-> 'r 'r) 's)" ) == "(-> (-> 'a 'a) (-> 'a 'a) (-> 'a 'a))" );
    assert( unify( "(-> 'a (-> 'b 'c))", "(-> (-> 'c int) 'd)" ) == "(-> (-> 'a int) (-> 'b 'a))" );

    assert( !unify( "int", "bool" ) );
    assert( !unify( "(-> int int)", "(-> int)" ) );
    assert( !unify( "'a", "(-> 'a int)" ) );
    assert( !unify( "(-> 'a 'b)", "(-> 'b (-> 'a int))" ) );
    assert( !unify( "(-> 'a 'a)", "(-> int bool)" ) );

    return 0;
}
