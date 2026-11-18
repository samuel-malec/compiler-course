#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <functional>
#include <map>
#include <set>

/* The abstract syntax used in this exercise is the same as in
 * ‹d3_rewrite›: immutable nodes, shared through ‹std::shared_ptr›.
 * Operators in ‹binary› are one of ‹+›, ‹-›, ‹*› and ‹/›. */

struct node;
using node_ptr = std::shared_ptr< const node >;

struct number   { long value; };
struct variable { std::string name; };
struct binary   { char op; node_ptr left, right; };
struct let      { std::string name; node_ptr value, body; };

struct node
{
    std::variant< number, variable, binary, let > v;
};

node_ptr num( long v ) { return std::make_shared< node >( number{ v } ); }
node_ptr var( std::string n ) { return std::make_shared< node >( variable{ n } ); }
node_ptr bin( char op, node_ptr l, node_ptr r )
{
    return std::make_shared< node >( binary{ op, l, r } );
}
node_ptr mk_let( std::string n, node_ptr v, node_ptr b )
{
    return std::make_shared< node >( let{ n, v, b } );
}

template< typename... fs > struct overloaded : fs... { using fs::operator()...; };

/* The function ‹show› prints a tree as an S-expression, and ‹sx›
 * does the opposite: it builds a tree from an S-expression. The
 * tests use both. You can use them too, but you shouldn't need to. */

std::string show( const node_ptr &n )
{
    return std::visit( overloaded
    {
        []( const number &x ) { return std::to_string( x.value ); },
        []( const variable &x ) { return x.name; },
        []( const binary &x )
        {
            return std::string( "(" ) + x.op + " " + show( x.left ) + " " +
                   show( x.right ) + ")";
        },
        []( const let &x )
        {
            return "(let " + x.name + " " + show( x.value ) + " " +
                   show( x.body ) + ")";
        }
    }, n->v );
}

std::string sx_token( std::string_view &s )
{
    while ( !s.empty() && s[ 0 ] == ' ' )
        s.remove_prefix( 1 );
    std::size_t n = s.empty() ? 0 : s[ 0 ] == '(' || s[ 0 ] == ')' ? 1 :
                    std::min( s.find_first_of( " ()" ), s.size() );
    std::string t( s.substr( 0, n ) );
    s.remove_prefix( n );
    return t;
}

node_ptr sx_read( std::string_view &s )
{
    std::string t = sx_token( s );
    if ( t != "(" )
        return t.find_first_not_of( "-0123456789" ) == t.npos && t != "-"
               ? num( std::stol( t ) ) : var( t );

    std::string head = sx_token( s );
    node_ptr result;

    if ( head == "let" )
    {
        std::string name = sx_token( s );
        node_ptr value = sx_read( s );
        result = mk_let( name, value, sx_read( s ) );
    }
    else
    {
        node_ptr left = sx_read( s );
        result = bin( head[ 0 ], left, sx_read( s ) );
    }

    sx_token( s ); /* the closing parenthesis */
    return result;
}

node_ptr sx( std::string_view s ) { return sx_read( s ); }

/* Look back at the functions you have written in this chapter: most
 * of them have the same shape. They recurse into the children, and
 * then combine the results of the children with the information in
 * the node itself. This shape can be captured by a single
 * higher-order function, usually called a «fold» (or a
 * «catamorphism», if you want to impress your friends).
 *
 * Write a function ‹fold( n, on_num, on_var, on_bin, on_let )›, where
 * the last 4 arguments are callables, which are used like this:
 *
 *  • for a ‹number› node, the result is ‹on_num( value )›,
 *  • for a ‹variable›, it is ‹on_var( name )›,
 *  • for a ‹binary› node, it is ‹on_bin( op, l, r )›, where ‹l› and
 *    ‹r› are the results of folding the left and right children,
 *  • for a ‹let›, it is ‹on_let( name, v, b )›, where ‹v› and ‹b› are
 *    the results of folding the value and the body.
 *
 * All 4 callables return the same type, which is also the return type
 * of ‹fold›. The tests then use ‹fold› to implement a few of the
 * functions from earlier exercises, and an «evaluator» (which is
 * where we are heading in the next chapter).
 *
 * A technical hint: since ‹fold› is recursive, the compiler cannot
 * deduce its return type from a ‹return› statement which calls
 * ‹fold› itself; spell the return type out using a trailing return
 * type, e.g. ‹-> decltype( on_num( 0L ) )›. */

// auto fold( const node_ptr &n, auto on_num, auto on_var, auto on_bin, auto on_let );

auto fold( const node_ptr &n, auto on_num, auto on_var, auto on_bin, auto on_let )
    -> decltype( on_num( 0L ) )
{
    using result = decltype( on_num( 0L ) );
    auto rec = [ & ]( const node_ptr &c ) -> result
    {
        return fold( c, on_num, on_var, on_bin, on_let );
    };

    return std::visit( overloaded
    {
        [ & ]( const number &x ) -> result { return on_num( x.value ); },
        [ & ]( const variable &x ) -> result { return on_var( x.name ); },
        [ & ]( const binary &x ) -> result
        {
            return on_bin( x.op, rec( x.left ), rec( x.right ) );
        },
        [ & ]( const let &x ) -> result
        {
            return on_let( x.name, rec( x.value ), rec( x.body ) );
        }
    }, n->v );
}

int main()
{
    using sset = std::set< std::string >;
    auto t = sx( "(let x (+ 1 2) (* x (- y x)))" );

    /* size */
    auto one = []( const auto & ) { return 1; };
    auto add = []( const auto &, int a, int b ) { return 1 + a + b; };
    assert( fold( t, one, one, add, add ) == 9 );

    /* show */
    auto s = fold( t,
        []( long v ) { return std::to_string( v ); },
        []( const std::string &n ) { return n; },
        []( char op, std::string a, std::string b )
        {
            return std::string( "(" ) + op + " " + a + " " + b + ")";
        },
        []( const std::string &n, std::string a, std::string b )
        {
            return "(let " + n + " " + a + " " + b + ")";
        } );
    assert( s == show( t ) );

    /* free variables */
    auto fv = fold( t,
        []( long ) { return sset{}; },
        []( const std::string &n ) { return sset{ n }; },
        []( char, sset a, sset b ) { a.merge( b ); return a; },
        []( const std::string &n, sset a, sset b )
        {
            b.erase( n );
            a.merge( b );
            return a;
        } );
    assert(( fv == sset{ "y" } ));

    /* evaluation: the result of the fold is a «function» which takes
     * an environment (the values of the free variables) */
    using env_t = std::map< std::string, long >;
    using fn = std::function< long( const env_t & ) >;

    auto compile = [ & ]( const node_ptr &n ) -> fn
    {
        return fold( n,
            []( long v ) -> fn { return [ = ]( const env_t & ) { return v; }; },
            []( const std::string &n ) -> fn
            {
                return [ = ]( const env_t &e ) { return e.at( n ); };
            },
            []( char op, fn a, fn b ) -> fn
            {
                return [ = ]( const env_t &e )
                {
                    long x = a( e ), y = b( e );
                    return op == '+' ? x + y : op == '-' ? x - y :
                           op == '*' ? x * y : x / y;
                };
            },
            []( const std::string &n, fn a, fn b ) -> fn
            {
                return [ = ]( const env_t &e )
                {
                    env_t inner = e;
                    inner[ n ] = a( e );
                    return b( inner );
                };
            } );
    };

    assert( compile( t )( { { "y", 10 } } ) == 21 );
    assert( compile( sx( "(let x 3 (* x (+ x 1)))" ) )( {} ) == 12 );
    assert( compile( sx( "(let x 1 (let x (+ x 1) x))" ) )( {} ) == 2 );

    return 0;
}
