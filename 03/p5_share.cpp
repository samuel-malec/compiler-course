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

/* Since our nodes are immutable, structurally equal sub-trees can be
 * represented by a single shared node – the tree becomes a «DAG»
 * (directed acyclic graph). This saves memory, and more importantly,
 * it makes it trivial to recognise «common sub-expressions»: two
 * sub-trees are equal if and only if they are the same pointer. The
 * technique is known as «hash consing».
 *
 * Write two functions:
 *
 *  • ‹distinct( n )› returns the number of «distinct» nodes (i.e.
 *    distinct addresses) reachable from ‹n›; a shared node is only
 *    counted once, no matter how many parents it has,
 *  • ‹share( n )› returns a tree which is structurally equal to ‹n›,
 *    but in which any two structurally equal sub-trees are the same
 *    node.
 *
 * Both should run in time ⟦O(k log k)⟧ where ⟦k⟧ is the number of
 * distinct nodes in the input – in particular, ‹share› must not
 * compare sub-trees pairwise. Hint: if the children of two nodes
 * have already been shared, then the two nodes are structurally
 * equal if and only if they are of the same kind, they have the same
 * operator/value/name, and their children are «the same pointers».
 * Keep a ‹std::map› from such ‘shallow descriptions’ to the nodes
 * you have already built. */

// std::size_t distinct( const node_ptr &n );
// node_ptr share( const node_ptr &n );

int main()
{
    auto t1 = sx( "(+ (* a b) (* a b))" );
    auto s1 = share( t1 );
    assert( show( s1 ) == show( t1 ) );
    assert( distinct( t1 ) == 7 );
    assert( distinct( s1 ) == 4 );
    auto &b1 = std::get< binary >( s1->v );
    assert( b1.left == b1.right );

    auto t2 = sx( "(let x (+ 1 2) (+ (+ 1 2) x))" );
    auto s2 = share( t2 );
    assert( show( s2 ) == show( t2 ) );
    assert( distinct( t2 ) == 9 );
    assert( distinct( s2 ) == 6 );

    assert( distinct( share( sx( "(+ 1 1)" ) ) ) == 2 );
    assert( distinct( share( sx( "(+ x 1)" ) ) ) == 3 );
    assert( distinct( share( sx( "(- (let x 1 x) (let y 1 y))" ) ) ) == 6 );
    assert( distinct( share( sx( "(- (+ a b) (- a b))" ) ) ) == 5 );

    /* A complete binary tree of depth 17, built without any sharing:
     * 2¹⁷ - 1 nodes, but only 17 distinct sub-trees. */
    auto full = []( auto self, int depth ) -> node_ptr
    {
        if ( depth == 1 )
            return var( "x" );
        return bin( '+', self( self, depth - 1 ), self( self, depth - 1 ) );
    };

    auto big = full( full, 17 );
    assert( distinct( big ) == ( 1u << 17 ) - 1 );
    auto shared = share( big );
    assert( distinct( shared ) == 17 );

    /* Sharing an already shared tree changes nothing. */
    assert( distinct( share( shared ) ) == 17 );

    return 0;
}
