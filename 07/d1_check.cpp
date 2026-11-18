#include <cassert>
#include <map>
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

/* In this demo, we write a type checker for the typed language from
 * the introduction. Types are S-expressions too, so we can reuse
 * ‹sexp› to represent them: ‹int› and ‹bool› are atoms, a function
 * type ‹(-> σ₁ … σₙ τ)› is a list. Two types are equal if they are
 * structurally equal, which, for S-expressions, is the same as having
 * the same written form. */

using type = sexp;

bool same( const type &a, const type &b ) { return write( a ) == write( b ); }

type atom( std::string a ) { return sexp{ a, {}, 0 }; }
const type int_t = atom( "int" ), bool_t = atom( "bool" );

struct type_error : std::runtime_error { using std::runtime_error::runtime_error; };

/* The typing context ‹Γ›. As in an interpreter, it is a chain, but we
 * can get away with copying a ‹std::map› – contexts are small and are
 * only extended at ‹let› and ‹fn›. */

using context = std::map< std::string, type >;

/* A type annotation must itself be a well-formed type. */

void check_type( const type &t )
{
    if ( t.is_atom() )
    {
        if ( !same( t, int_t ) && !same( t, bool_t ) )
            throw type_error( "unknown type " + t.atom );
        return;
    }
    if ( t.items.size() < 2 || t.items[ 0 ].atom != "->" )
        throw type_error( "malformed type" );
    for ( std::size_t i = 1; i < t.items.size(); ++ i )
        check_type( t.items[ i ] );
}

/* A helper which states an expectation: ‹e› must have type ‹t›. Most
 * rules are built from such expectations. */

type check( const sexp &e, const context &ctx );

void expect( const sexp &e, const context &ctx, const type &t )
{
    type actual = check( e, ctx );
    if ( !same( actual, t ) )
        throw type_error( "expected " + write( t ) + ", got " + write( actual ) +
                          " in " + write( e ) );
}

type check( const sexp &e, const context &ctx )
{
    /* Literals and variables: the axioms. */
    if ( e.is_atom() )
    {
        if ( e.atom == "#t" || e.atom == "#f" )
            return bool_t;
        if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos )
            return int_t;
        if ( auto it = ctx.find( e.atom ); it != ctx.end() )
            return it->second;
        throw type_error( "unbound variable " + e.atom );
    }

    const auto &it = e.items;
    const std::string &head = it.at( 0 ).atom;

    if ( head == "+" || head == "-" || head == "*" || head == "/" )
    {
        expect( it.at( 1 ), ctx, int_t );
        expect( it.at( 2 ), ctx, int_t );
        return int_t;
    }

    if ( head == "<" || head == "<=" || head == ">" || head == ">=" )
    {
        expect( it.at( 1 ), ctx, int_t );
        expect( it.at( 2 ), ctx, int_t );
        return bool_t;
    }

    /* Equality is ‘polymorphic’ over ‹int› and ‹bool›: the first
     * operand decides which, the second must match. */
    if ( head == "=" || head == "!=" )
    {
        type t = check( it.at( 1 ), ctx );
        if ( t.is_list() )
            throw type_error( "cannot compare functions" );
        expect( it.at( 2 ), ctx, t );
        return bool_t;
    }

    if ( head == "and" || head == "or" )
    {
        expect( it.at( 1 ), ctx, bool_t );
        expect( it.at( 2 ), ctx, bool_t );
        return bool_t;
    }

    if ( head == "not" )
    {
        expect( it.at( 1 ), ctx, bool_t );
        return bool_t;
    }

    /* ‹if›: «both» branches are checked, and they must agree. */
    if ( head == "if" )
    {
        expect( it.at( 1 ), ctx, bool_t );
        type t = check( it.at( 2 ), ctx );
        expect( it.at( 3 ), ctx, t );
        return t;
    }

    if ( head == "let" )
    {
        context inner = ctx;
        inner.insert_or_assign( it.at( 1 ).atom, check( it.at( 2 ), ctx ) );
        return check( it.at( 3 ), inner );
    }

    /* ‹fn›: check the body «once», assuming that the parameters have
     * their declared types. The result is a function type built from
     * the annotations and the type of the body. */
    if ( head == "fn" )
    {
        context inner = ctx;
        type result{ "", { atom( "->" ) }, 0 };

        for ( const auto &p : it.at( 1 ).items )
        {
            check_type( p.items.at( 1 ) );
            inner.insert_or_assign( p.items.at( 0 ).atom, p.items[ 1 ] );
            result.items.push_back( p.items[ 1 ] );
        }

        result.items.push_back( check( it.at( 2 ), inner ) );
        return result;
    }

    /* A call: the callee must have a function type, and each argument
     * must have the corresponding parameter type. */
    type f = check( it[ 0 ], ctx );
    if ( f.is_atom() )
        throw type_error( "not a function: " + write( it[ 0 ] ) );
    if ( f.items.size() != it.size() + 1 )
        throw type_error( "wrong number of arguments" );
    for ( std::size_t i = 1; i < it.size(); ++ i )
        expect( it[ i ], ctx, f.items[ i ] );
    return f.items.back();
}

std::string type_of( std::string_view src )
{
    try { return write( check( read( src ), {} ) ); }
    catch ( const type_error &e ) { return e.what(); }
}

int main() /* demo */
{
    assert( type_of( "(+ 1 2)" ) == "int" );
    assert( type_of( "(if (< 1 2) #t (not #t))" ) == "bool" );
    assert( type_of( "(fn ((x int)) (* x x))" ) == "(-> int int)" );
    assert( type_of( "(fn ((f (-> int bool)) (x int)) (if (f x) 1 0))" ) ==
            "(-> (-> int bool) int int)" );
    assert( type_of( "(let twice (fn ((f (-> int int)) (x int)) (f (f x)))"
                     " (twice (fn ((n int)) (+ n 1)) 5))" ) == "int" );
    assert( type_of( "(fn ((x int)) (fn ((y int)) (= x y)))" ) == "(-> int (-> int bool))" );

    assert( type_of( "(+ 1 #t)" ) == "expected int, got bool in #t" );
    assert( type_of( "(if #t 1 #f)" ) == "expected int, got bool in #f" ); /* conservative! */
    assert( type_of( "((fn ((x int)) x) #t)" ) == "expected int, got bool in #t" );
    assert( type_of( "(1 2)" ) == "not a function: 1" );
    assert( type_of( "((fn ((x int)) x))" ) == "wrong number of arguments" );
    assert( type_of( "(fn ((x num)) x)" ) == "unknown type num" );
    assert( type_of( "(= (fn ((x int)) x) 1)" ) == "cannot compare functions" );
    assert( type_of( "y" ) == "unbound variable y" );

    return 0;
}
