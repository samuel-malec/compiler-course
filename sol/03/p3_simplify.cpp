#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
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

/* Write a function ‹simplify›, which repeatedly applies the following
 * rules anywhere in the tree, until none of them applies anymore
 * (‹A› and ‹B› stand for arbitrary sub-trees, ‹n› and ‹m› for
 * numbers):
 *
 *  1. constant folding: ‹n + m›, ‹n - m› and ‹n * m› are replaced by
 *     the result; ‹n / m› too, unless ‹m› is zero (division truncates
 *     towards zero, as in C++),
 *  2. neutral elements: ‹A + 0 → A›, ‹0 + A → A›, ‹A - 0 → A›,
 *     ‹A * 1 → A›, ‹1 * A → A› and ‹A / 1 → A›,
 *  3. zero: ‹A * 0 → 0› and ‹0 * A → 0›,
 *  4. self-cancellation: ‹A - A → 0›, where both sides are
 *     «structurally equal» (the same tree, though not necessarily the
 *     same pointer),
 *  5. unused bindings: ‹let x = A in B → B›, if ‹x› is not free in
 *     ‹B› (see ‹e3_free›),
 *  6. trivial bindings: ‹let x = A in x → A›.
 *
 * Rules 3 and 4 are only valid because our expressions have no side
 * effects and cannot fail – think about why, and about what would go
 * wrong otherwise (e.g. with ‹(1 / 0) * 0›). Hint: simplifying the
 * children before the node itself (bottom-up, as in ‹d3_rewrite›)
 * makes it much easier to reach a state where no rule applies. */

// node_ptr simplify( const node_ptr &n );

bool equal( const node_ptr &a, const node_ptr &b )
{
    if ( a == b )
        return true;

    return std::visit( overloaded
    {
        []( const number &x, const number &y ) { return x.value == y.value; },
        []( const variable &x, const variable &y ) { return x.name == y.name; },
        []( const binary &x, const binary &y )
        {
            return x.op == y.op && equal( x.left, y.left ) && equal( x.right, y.right );
        },
        []( const let &x, const let &y )
        {
            return x.name == y.name && equal( x.value, y.value ) && equal( x.body, y.body );
        },
        []( const auto &, const auto & ) { return false; }
    }, a->v, b->v );
}

bool is_free( const std::string &x, const node_ptr &n )
{
    return std::visit( overloaded
    {
        []( const number & ) { return false; },
        [ & ]( const variable &v ) { return v.name == x; },
        [ & ]( const binary &b ) { return is_free( x, b.left ) || is_free( x, b.right ); },
        [ & ]( const let &l )
        {
            return is_free( x, l.value ) || ( l.name != x && is_free( x, l.body ) );
        }
    }, n->v );
}

/* Apply the rules to a single node whose children are already fully
 * simplified. If a rule fires, the result may enable further rules,
 * so we simplify it again (it is always smaller, so this ends). */

node_ptr simplify( const node_ptr &n );

node_ptr rule( const node_ptr &n )
{
    if ( auto l = std::get_if< let >( &n->v ) )
    {
        if ( !is_free( l->name, l->body ) )
            return l->body;
        if ( auto v = std::get_if< variable >( &l->body->v ); v && v->name == l->name )
            return l->value;
        return n;
    }

    auto b = std::get_if< binary >( &n->v );
    if ( !b )
        return n;

    auto ln = std::get_if< number >( &b->left->v );
    auto rn = std::get_if< number >( &b->right->v );
    auto is = []( const number *x, long v ) { return x && x->value == v; };
    char op = b->op;

    if ( ln && rn )
        switch ( op )
        {
            case '+': return num( ln->value + rn->value );
            case '-': return num( ln->value - rn->value );
            case '*': return num( ln->value * rn->value );
            case '/': if ( rn->value != 0 ) return num( ln->value / rn->value );
                      break;
        }

    if ( ( op == '+' || op == '-' ) && is( rn, 0 ) ) return b->left;
    if ( op == '+' && is( ln, 0 ) ) return b->right;
    if ( ( op == '*' || op == '/' ) && is( rn, 1 ) ) return b->left;
    if ( op == '*' && is( ln, 1 ) ) return b->right;
    if ( op == '*' && ( is( ln, 0 ) || is( rn, 0 ) ) ) return num( 0 );
    if ( op == '-' && equal( b->left, b->right ) ) return num( 0 );

    return n;
}

node_ptr simplify( const node_ptr &n )
{
    node_ptr rebuilt = std::visit( overloaded
    {
        [ & ]( const binary &b ) { return bin( b.op, simplify( b.left ), simplify( b.right ) ); },
        [ & ]( const let &l ) { return mk_let( l.name, simplify( l.value ), simplify( l.body ) ); },
        [ & ]( const auto & ) { return n; }
    }, n->v );

    node_ptr result = rule( rebuilt );
    return result == rebuilt ? result : simplify( result );
}

int main()
{
    auto s = []( std::string_view e ) { return show( simplify( sx( e ) ) ); };

    assert( s( "x" ) == "x" );
    assert( s( "(+ 1 2)" ) == "3" );
    assert( s( "(* (+ 1 2) (- 5 1))" ) == "12" );
    assert( s( "(/ 7 2)" ) == "3" );
    assert( s( "(/ -7 2)" ) == "-3" );
    assert( s( "(/ 7 0)" ) == "(/ 7 0)" );
    assert( s( "(+ x 0)" ) == "x" );
    assert( s( "(+ 0 x)" ) == "x" );
    assert( s( "(- x 0)" ) == "x" );
    assert( s( "(- 0 x)" ) == "(- 0 x)" );
    assert( s( "(* (+ x 0) 1)" ) == "x" );
    assert( s( "(/ x 1)" ) == "x" );
    assert( s( "(* x 0)" ) == "0" );
    assert( s( "(* 0 (+ x y))" ) == "0" );
    assert( s( "(- (+ x y) (+ x y))" ) == "0" );
    assert( s( "(- (+ x y) (+ y x))" ) == "(- (+ x y) (+ y x))" );
    assert( s( "(- (* x 1) x)" ) == "0" );
    assert( s( "(let x 1 y)" ) == "y" );
    assert( s( "(let x (+ 1 1) (* x x))" ) == "(let x 2 (* x x))" );
    assert( s( "(let x (+ a 1) x)" ) == "(+ a 1)" );
    assert( s( "(let y (+ 1 2) (- y y))" ) == "0" );
    assert( s( "(+ (let x 3 x) (* 2 2))" ) == "7" );
    assert( s( "(let x 1 (let x 2 x))" ) == "2" );
    assert( s( "(* a (+ b (- c c)))" ) == "(* a b)" );

    return 0;
}
