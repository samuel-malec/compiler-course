#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <expected>
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

/* A type checker is only as good as its error messages. Write a
 * checker for the language of ‹p1_lambda› which, for an ill-typed
 * program, reports «where» the problem is and «what» it is. The
 * result is either the type of the program (written with ‹write›) or
 * a message ‹LINE:COLUMN: TEXT›, where the position (numbered from 1,
 * columns counted in bytes) is that of the first character of the
 * offending sub-expression, and ‹TEXT› is one of:
 *
 *  • ‹unbound variable X› – at the variable,
 *  • ‹expected T, got U› – at a sub-expression which has type ‹U›, but
 *    must have type ‹T›: an operand of an operator, of ‹and›, ‹or› or
 *    ‹not›, the condition of ‹if›, the «second» branch of ‹if› (which
 *    must have the type of the first one), the second operand of ‹=› or
 *    ‹!=› (which must have the type of the first one), or an argument
 *    of a call,
 *  • ‹cannot compare T› – at the first operand of ‹=› or ‹!=›, if its
 *    type ‹T› is a function type,
 *  • ‹not a function: T› – at the callee of a call, which has type ‹T›,
 *  • ‹wrong number of arguments› – at the call,
 *  • ‹bad type› – at a malformed type annotation,
 *  • ‹bad form› – at a list which does not have the shape of any
 *    expression; if the problem is a malformed parameter list of ‹fn›
 *    or a repeated parameter name, at the ‹fn› expression,
 *  • ‹syntax error› – at ‹1:1›, if the program cannot be ‹read›.
 *
 * Types in messages are written with ‹write›. The sub-expressions are
 * checked from left to right (callee first, then the arguments), and
 * only the first error is reported. */

// std::expected< std::string, std::string > check( std::string_view program );

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

std::expected< std::string, std::string > check( std::string_view program )
{
    sexp e;
    try { e = read( program ); }
    catch ( const std::invalid_argument & ) { return std::unexpected( "1:1: syntax error" ); }

    try { return write( check( e, {} ) ); }
    catch ( const type_error &err )
    {
        int line = 1, col = 1;
        for ( std::size_t i = 0; i < err.offset; ++ i )
            program[ i ] == '\n' ? ( ++ line, col = 1 ) : ++ col;
        return std::unexpected( std::to_string( line ) + ":" + std::to_string( col ) + ": " + err.text );
    }
}

int main()
{
    auto err = []( std::string_view p ) { return check( p ).error(); };

    assert( check( "(fn ((x int)) (< x 1))" ) == "(-> int bool)" );

    assert( err( "(+ 1 #t)" ) == "1:6: expected int, got bool" );
    assert( err( "(let f (fn ((x int)) x)\n  (f #f))" ) == "2:6: expected int, got bool" );
    assert( err( "(if (< 1 2)\n    1\n    (< 1 2))" ) == "3:5: expected int, got bool" );
    assert( err( "(if 1 2 3)" ) == "1:5: expected bool, got int" );
    assert( err( "(+ y 1)" ) == "1:4: unbound variable y" );
    assert( err( "(let g (fn ((h (-> int int))) (h 1 2)) g)" ) == "1:31: wrong number of arguments" );
    assert( err( "(let n 5 (n 1))" ) == "1:11: not a function: int" );
    assert( err( "(= (fn () 1) 2)" ) == "1:4: cannot compare (-> int)" );
    assert( err( "(= 1 #t)" ) == "1:6: expected int, got bool" );
    assert( err( "(fn ((x (-> int float))) x)" ) == "1:9: bad type" );
    assert( err( "(fn ((x int) (x int)) x)" ) == "1:1: bad form" );
    assert( err( "(+ 1 (let x 2))" ) == "1:6: bad form" );
    assert( err( "(not (and #t (fn () #t)))" ) == "1:14: expected bool, got (-> bool)" );
    assert( err( "((fn ((x bool)) x) (+ 1 #f))" ) == "1:25: expected int, got bool" );
    assert( err( "(+ 1 2" ) == "1:1: syntax error" );

    return 0;
}
