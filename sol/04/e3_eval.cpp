#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

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

/* Write an evaluator for the abstract syntax from Chapter 3. The
 * function ‹eval( tree, env )› returns the value of ‹tree›, where the
 * values of free variables are given by ‹env›. Division truncates
 * towards zero. Return ‹std::nullopt› on division by zero, or if a
 * variable is not bound. */

// std::optional< long > eval( const node_ptr &n, const std::map< std::string, long > &env );

std::optional< long > eval( const node_ptr &n, const std::map< std::string, long > &env )
{
    return std::visit( overloaded
    {
        []( const number &x ) -> std::optional< long > { return x.value; },
        [ & ]( const variable &x ) -> std::optional< long >
        {
            auto it = env.find( x.name );
            if ( it == env.end() )
                return std::nullopt;
            return it->second;
        },
        [ & ]( const binary &b ) -> std::optional< long >
        {
            auto l = eval( b.left, env ), r = eval( b.right, env );
            if ( !l || !r || ( b.op == '/' && *r == 0 ) )
                return std::nullopt;
            switch ( b.op )
            {
                case '+': return *l + *r;
                case '-': return *l - *r;
                case '*': return *l * *r;
                default:  return *l / *r;
            }
        },
        [ & ]( const let &l ) -> std::optional< long >
        {
            auto v = eval( l.value, env );
            if ( !v )
                return std::nullopt;
            auto inner = env;  /* a copy; see ‹d1_eval› for a cheaper way */
            inner[ l.name ] = *v;
            return eval( l.body, inner );
        }
    }, n->v );
}

int main()
{
    std::map< std::string, long > env{ { "x", 3 }, { "y", 4 } };
    auto ev = [ & ]( std::string_view s ) { return eval( sx( s ), env ); };

    assert( ev( "42" ) == 42 );
    assert( ev( "x" ) == 3 );
    assert( ev( "(+ x (* y 2))" ) == 11 );
    assert( ev( "(/ -7 2)" ) == -3 );
    assert( ev( "(let z 10 (- z x))" ) == 7 );
    assert( ev( "(let x 10 x)" ) == 10 );
    assert( ev( "(+ (let x 10 x) x)" ) == 13 );
    assert( ev( "(let x (+ x 1) (let x (* x x) x))" ) == 16 );

    assert( !ev( "z" ) );
    assert( !ev( "(/ 1 (- x 3))" ) );
    assert( !ev( "(let a 1 (+ a b))" ) );
    assert( !ev( "(let a (/ 1 0) 5)" ) );

    return 0;
}
