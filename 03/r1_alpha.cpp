#include <algorithm>
#include <cassert>
#include <memory>
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

/* The names of bound variables are irrelevant: ‹let x = 1 in x + y›
 * and ‹let z = 1 in z + y› mean exactly the same thing. Two
 * expressions which only differ in the names of bound variables are
 * called «α-equivalent». (Free variables, on the other hand, matter:
 * ‹x + y› and ‹x + z› are not α-equivalent.)
 *
 * Write a function ‹alpha_equiv› which decides whether two trees are
 * α-equivalent. Careful with shadowing: in ‹let x = 1 in let x = 2
 * in x›, the ‹x› in the body refers to the «inner» ‹let›.
 *
 * Hint: walk both trees at once, and keep track of the ‘binding
 * depth’ of each bound variable – the variables bound by the ‹let›s
 * which enclose the current position, counted from the innermost
 * one. Two bound variables correspond to each other if they have the
 * same depth. (This is the idea behind «de Bruijn indices», a
 * nameless representation of bound variables.) */

// bool alpha_equiv( const node_ptr &a, const node_ptr &b );

int main()
{
    auto eq = []( std::string_view a, std::string_view b )
    {
        return alpha_equiv( sx( a ), sx( b ) );
    };

    assert( eq( "1", "1" ) );
    assert( !eq( "1", "2" ) );
    assert( eq( "x", "x" ) );
    assert( !eq( "x", "y" ) );
    assert( eq( "(+ x 1)", "(+ x 1)" ) );
    assert( !eq( "(+ x 1)", "(- x 1)" ) );
    assert( eq( "(let x 1 x)", "(let y 1 y)" ) );
    assert( eq( "(let x 1 y)", "(let z 1 y)" ) );
    assert( !eq( "(let x 1 x)", "(let y 1 x)" ) );
    assert( !eq( "(let x 1 y)", "(let y 1 y)" ) );
    assert( eq( "(let x 1 (let y 2 (+ x y)))", "(let a 1 (let b 2 (+ a b)))" ) );
    assert( !eq( "(let x 1 (let y 2 (+ x y)))", "(let a 1 (let b 2 (+ b a)))" ) );
    assert( eq( "(let x 1 (let x 2 x))", "(let a 1 (let b 2 b))" ) );
    assert( !eq( "(let x 1 (let x 2 x))", "(let a 1 (let b 2 a))" ) );
    assert( eq( "(let x x x)", "(let y x y)" ) );
    assert( !eq( "(let x x x)", "(let y y y)" ) );
    assert( eq( "(+ (let x 1 x) x)", "(+ (let y 1 y) x)" ) );
    assert( !eq( "(+ (let x 1 x) x)", "(+ (let y 1 y) y)" ) );
    assert( !eq( "(let x 1 x)", "(+ 1 1)" ) );

    return 0;
}
