#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <map>
#include <utility>
#include <vector>

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

/* In ‹p3_simplify›, the simplification rules were hard-wired into
 * the code. Many compilers (and computer algebra systems) instead
 * let the rules be given as «data» – pairs of patterns, like
 * ‹?a + 0 → ?a›. In this exercise, you will write the engine which
 * applies such rules.
 *
 * Both sides of a rule are trees, in which «pattern variables» are
 * represented by ‹variable› nodes whose name starts with ‹?›. A
 * pattern «matches» a tree as follows:
 *
 *  • a pattern variable matches any tree; if the same pattern
 *    variable appears more than once in the pattern, all the trees it
 *    matches must be structurally equal (so ‹?a - ?a› matches ‹(x +
 *    1) - (x + 1)›, but not ‹x - y›),
 *  • a number matches the same number, an ordinary variable matches
 *    a variable with the same name,
 *  • a binary node matches a binary node with the same operator,
 *    whose children match the children of the pattern,
 *  • a ‹let› matches a ‹let› which binds the same name (binders are
 *    never pattern variables), with matching value and body.
 *
 * The result of a successful match is a «substitution» – a map from
 * pattern variables to the trees they matched. To apply a rule, the
 * right-hand side is «instantiated» by replacing each pattern
 * variable with the tree it matched. (You can assume that every
 * pattern variable on the right-hand side also appears on the left.)
 *
 * Write a function ‹rewrite( tree, rules )›, which «normalizes» the
 * tree: first, it normalizes all the children; then it tries the
 * rules «in order» on the node itself; if a rule matches, the
 * instantiated right-hand side is normalized again (from scratch,
 * including its children) and that is the result; if no rule matches,
 * the node (with normalized children) is the result. You can assume
 * that the rules are such that normalization terminates. */

using rule = std::pair< node_ptr, node_ptr >;

// node_ptr rewrite( const node_ptr &t, const std::vector< rule > &rules );

bool equal( const node_ptr &a, const node_ptr &b )
{
    return a == b || std::visit( overloaded
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

using bindings = std::map< std::string, node_ptr >;

bool match( const node_ptr &pat, const node_ptr &t, bindings &b )
{
    if ( auto v = std::get_if< variable >( &pat->v ); v && v->name.starts_with( "?" ) )
    {
        auto [ it, fresh ] = b.emplace( v->name, t );
        return fresh || equal( it->second, t );
    }

    return std::visit( overloaded
    {
        []( const number &x, const number &y ) { return x.value == y.value; },
        []( const variable &x, const variable &y ) { return x.name == y.name; },
        [ & ]( const binary &x, const binary &y )
        {
            return x.op == y.op && match( x.left, y.left, b ) && match( x.right, y.right, b );
        },
        [ & ]( const let &x, const let &y )
        {
            return x.name == y.name && match( x.value, y.value, b ) &&
                   match( x.body, y.body, b );
        },
        []( const auto &, const auto & ) { return false; }
    }, pat->v, t->v );
}

node_ptr instantiate( const node_ptr &pat, const bindings &b )
{
    return std::visit( overloaded
    {
        [ & ]( const variable &v ) { return b.contains( v.name ) ? b.at( v.name ) : pat; },
        [ & ]( const binary &x ) { return bin( x.op, instantiate( x.left, b ), instantiate( x.right, b ) ); },
        [ & ]( const let &x ) { return mk_let( x.name, instantiate( x.value, b ), instantiate( x.body, b ) ); },
        [ & ]( const number & ) { return pat; }
    }, pat->v );
}

node_ptr rewrite( const node_ptr &t, const std::vector< rule > &rules )
{
    node_ptr n = std::visit( overloaded
    {
        [ & ]( const binary &x ) { return bin( x.op, rewrite( x.left, rules ), rewrite( x.right, rules ) ); },
        [ & ]( const let &x ) { return mk_let( x.name, rewrite( x.value, rules ), rewrite( x.body, rules ) ); },
        [ & ]( const auto & ) { return t; }
    }, t->v );

    for ( const auto &[ lhs, rhs ] : rules )
    {
        bindings b;
        if ( match( lhs, n, b ) )
            return rewrite( instantiate( rhs, b ), rules );
    }

    return n;
}

int main()
{
    auto r = []( std::string_view l, std::string_view r ) { return rule{ sx( l ), sx( r ) }; };

    std::vector< rule > algebra =
    {
        r( "(+ ?a 0)", "?a" ),
        r( "(+ 0 ?a)", "?a" ),
        r( "(* ?a 1)", "?a" ),
        r( "(- ?a ?a)", "0" ),
        r( "(* ?a (+ ?b ?c))", "(+ (* ?a ?b) (* ?a ?c))" ),
    };

    auto rw = [ & ]( std::string_view t ) { return show( rewrite( sx( t ), algebra ) ); };

    assert( rw( "x" ) == "x" );
    assert( rw( "(+ x 0)" ) == "x" );
    assert( rw( "(+ 0 (+ x 0))" ) == "x" );
    assert( rw( "(* x (+ y (- z z)))" ) == "(* x y)" );
    assert( rw( "(- (+ x 1) (+ x 1))" ) == "0" );
    assert( rw( "(- x y)" ) == "(- x y)" );
    assert( rw( "(* 2 (+ a b))" ) == "(+ (* 2 a) (* 2 b))" );
    assert( rw( "(* x (+ (+ a b) c))" ) == "(+ (+ (* x a) (* x b)) (* x c))" );
    assert( rw( "(* (- q q) (+ (* a 1) 0))" ) == "(* 0 a)" );
    assert( rw( "(let x (+ 1 0) (- x x))" ) == "(let x 1 0)" );

    /* Rules with lets and specific variables. */
    std::vector< rule > lets =
    {
        r( "(let x ?v x)", "?v" ),
        r( "(+ y ?a)", "(+ ?a y)" ),
    };

    assert( show( rewrite( sx( "(let x (+ y 1) x)" ), lets ) ) == "(+ 1 y)" );
    assert( show( rewrite( sx( "(let z (+ y 1) z)" ), lets ) ) == "(let z (+ 1 y) z)" );

    /* Order matters: the first matching rule wins. */
    std::vector< rule > first = { r( "(* ?a ?b)", "1" ), r( "(* ?a ?a)", "2" ) };
    assert( show( rewrite( sx( "(* x x)" ), first ) ) == "1" );

    return 0;
}
