#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
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

/* Write a type checker for the complete typed language from the
 * introduction, including functions and calls. The function
 * ‹type_of› returns the type of the expression (written with
 * ‹write›), or ‹std::nullopt› if the expression is ill-typed or
 * malformed. In particular, the following are errors:
 *
 *  • a parameter annotation which is not a valid type (a valid type is
 *    ‹int›, ‹bool›, or ‹(-> σ₁ … σₙ τ)› with valid ‹σᵢ› and ‹τ›),
 *  • two parameters of the same function with the same name,
 *  • comparing (with ‹=› or ‹!=›) two values of a function type,
 *  • calling something which is not a function, or with a wrong
 *    number of arguments, or with an argument of a wrong type. */

// std::optional< std::string > type_of( std::string_view expr );

struct type_error { std::size_t offset; std::string text; };

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
bool same( const type &a, const type &b ) { return write( a ) == write( b ); }

bool valid_type( const type &t )
{
    if ( t.is_atom() )
        return t.atom == "int" || t.atom == "bool";
    if ( t.items.size() < 2 || t.items[ 0 ].atom != "->" || !t.items[ 0 ].is_atom() )
        return false;
    for ( std::size_t i = 1; i < t.items.size(); ++ i )
        if ( !valid_type( t.items[ i ] ) )
            return false;
    return true;
}

using context = std::map< std::string, type >;

type check( const sexp &e, const context &ctx );

void expect( const sexp &e, const context &ctx, const type &t )
{
    type u = check( e, ctx );
    if ( !same( t, u ) )
        throw type_error{ e.offset, "expected " + write( t ) + ", got " + write( u ) };
}

type check( const sexp &e, const context &ctx )
{
    if ( e.is_atom() )
    {
        if ( e.atom == "#t" || e.atom == "#f" ) return atom( "bool" );
        if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos ) return atom( "int" );
        if ( auto it = ctx.find( e.atom ); it != ctx.end() ) return it->second;
        throw type_error{ e.offset, "unbound variable " + e.atom };
    }

    const auto &it = e.items;
    auto bad = [ & ] { return type_error{ e.offset, "bad form" }; };
    if ( it.empty() ) throw bad();
    const std::string &h = it[ 0 ].is_atom() ? it[ 0 ].atom : "";
    auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw bad(); };
    const type int_t = atom( "int" ), bool_t = atom( "bool" );

    if ( h == "+" || h == "-" || h == "*" || h == "/" || h == "<" || h == "<=" || h == ">" || h == ">=" )
    {
        arity( 2 );
        expect( it[ 1 ], ctx, int_t );
        expect( it[ 2 ], ctx, int_t );
        return ( h == "+" || h == "-" || h == "*" || h == "/" ) ? int_t : bool_t;
    }
    if ( h == "=" || h == "!=" )
    {
        arity( 2 );
        type t = check( it[ 1 ], ctx );
        if ( t.is_list() )
            throw type_error{ it[ 1 ].offset, "cannot compare " + write( t ) };
        expect( it[ 2 ], ctx, t );
        return bool_t;
    }
    if ( h == "and" || h == "or" || h == "not" )
    {
        arity( h == "not" ? 1 : 2 );
        for ( std::size_t i = 1; i < it.size(); ++ i )
            expect( it[ i ], ctx, bool_t );
        return bool_t;
    }
    if ( h == "if" )
    {
        arity( 3 );
        expect( it[ 1 ], ctx, bool_t );
        type t = check( it[ 2 ], ctx );
        expect( it[ 3 ], ctx, t );
        return t;
    }
    if ( h == "let" )
    {
        arity( 3 );
        if ( !it[ 1 ].is_atom() ) throw bad();
        context inner = ctx;
        inner.insert_or_assign( it[ 1 ].atom, check( it[ 2 ], ctx ) );
        return check( it[ 3 ], inner );
    }
    if ( h == "fn" )
    {
        arity( 2 );
        if ( !it[ 1 ].is_list() ) throw bad();
        context inner = ctx;
        type out{ "", { atom( "->" ) }, 0 };
        std::set< std::string > seen;
        for ( const auto &p : it[ 1 ].items )
        {
            if ( !p.is_list() || p.items.size() != 2 || !p.items[ 0 ].is_atom() ||
                 !seen.insert( p.items[ 0 ].atom ).second )
                throw bad();
            if ( !valid_type( p.items[ 1 ] ) )
                throw type_error{ p.items[ 1 ].offset, "bad type" };
            inner.insert_or_assign( p.items[ 0 ].atom, p.items[ 1 ] );
            out.items.push_back( p.items[ 1 ] );
        }
        out.items.push_back( check( it[ 2 ], inner ) );
        return out;
    }

    type f = check( it[ 0 ], ctx );
    if ( f.is_atom() )
        throw type_error{ it[ 0 ].offset, "not a function: " + write( f ) };
    if ( f.items.size() != it.size() + 1 )
        throw type_error{ e.offset, "wrong number of arguments" };
    for ( std::size_t i = 1; i < it.size(); ++ i )
        expect( it[ i ], ctx, f.items[ i ] );
    return f.items.back();
}

std::optional< std::string > type_of( std::string_view expr )
{
    try { return write( check( read( expr ), {} ) ); }
    catch ( const type_error & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    assert( type_of( "(fn ((x int)) (+ x 1))" ) == "(-> int int)" );
    assert( type_of( "(fn () #t)" ) == "(-> bool)" );
    assert( type_of( "((fn ((x int) (y bool)) (if y x 0)) 5 #t)" ) == "int" );
    assert( type_of( "(fn ((f (-> int int))) (fn ((x int)) (f (f x))))" ) ==
            "(-> (-> int int) (-> int int))" );
    assert( type_of( "(let compose (fn ((f (-> int bool)) (g (-> int int)))"
                     " (fn ((x int)) (f (g x)))) (compose (fn ((n int)) (< n 0)) (fn ((n int)) (- 0 n))))" ) ==
            "(-> int bool)" );
    assert( type_of( "(let x 1 (fn ((x bool)) x))" ) == "(-> bool bool)" );
    assert( type_of( "((fn ((f (-> int))) (f)) (fn () 7))" ) == "int" );
    assert( type_of( "(if #t (fn ((x int)) x) (fn ((y int)) (* y 2)))" ) == "(-> int int)" );

    assert( !type_of( "(fn ((x int)) (x 1))" ) );
    assert( !type_of( "((fn ((x int)) x) #t)" ) );
    assert( !type_of( "((fn ((x int)) x) 1 2)" ) );
    assert( !type_of( "(fn ((x int) (x bool)) x)" ) );
    assert( !type_of( "(fn ((x float)) x)" ) );
    assert( !type_of( "(fn ((x (->))) x)" ) );
    assert( !type_of( "(= (fn () 1) (fn () 1))" ) );
    assert( !type_of( "(if #t (fn ((x int)) x) (fn ((x bool)) x))" ) );
    assert( !type_of( "(fn (x) x)" ) );

    return 0;
}
