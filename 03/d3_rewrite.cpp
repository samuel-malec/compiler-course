#include <cassert>
#include <memory>
#include <string>
#include <variant>

/* Compilers transform trees all the time: they simplify expressions,
 * replace one construct with another (‘desugaring’), inline
 * functions, and so on. In this demo, we will look at a
 * representation which makes transformations particularly easy:
 * «immutable» trees with «shared» sub-trees.
 *
 * The node types are the same as in ‹d2_variant›, but children are
 * held by ‹std::shared_ptr< const node >›. Since nodes can never
 * change once they are built, it is perfectly safe for a single node
 * to be a child of several parents, or a part of several different
 * trees. A transformation can therefore build a new tree which
 * «reuses» all the parts of the old tree that did not change: if we
 * simplify a single leaf in a large tree, only the nodes on the path
 * from the root to that leaf need to be rebuilt. */

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

/* Most transformations have the same structure: recurse into the
 * children, then do something with the node itself. We can factor out
 * the first part into a helper, ‹map_children›, which applies a
 * function to each child and rebuilds the node – «unless» none of the
 * children changed, in which case it returns the original node. This
 * is what preserves sharing. */

node_ptr map_children( const node_ptr &n, auto f )
{
    if ( auto b = std::get_if< binary >( &n->v ) )
    {
        auto l = f( b->left ), r = f( b->right );
        if ( l == b->left && r == b->right )
            return n;
        return bin( b->op, l, r );
    }

    if ( auto l = std::get_if< let >( &n->v ) )
    {
        auto v = f( l->value ), b = f( l->body );
        if ( v == l->value && b == l->body )
            return n;
        return mk_let( l->name, v, b );
    }

    return n; /* leaves have no children */
}

/* With ‹map_children› in hand, a «bottom-up» transformation (one
 * which first transforms the children, then the node) is just a
 * couple of lines: */

node_ptr bottom_up( const node_ptr &n, auto rule )
{
    auto rec = [ & ]( const node_ptr &c ) { return bottom_up( c, rule ); };
    return rule( map_children( n, rec ) );
}

/* Now, a simplification «rule»: a function which looks at a single
 * node (whose children have already been simplified) and either
 * returns a simpler node, or the node itself. We implement constant
 * folding (‹2 * 3 → 6›) and two identities (‹x + 0 → x› and
 * ‹x * 1 → x›). */

node_ptr simplify_rule( const node_ptr &n )
{
    auto b = std::get_if< binary >( &n->v );
    if ( !b )
        return n;

    auto l = std::get_if< number >( &b->left->v );
    auto r = std::get_if< number >( &b->right->v );

    if ( l && r )
        switch ( b->op )
        {
            case '+': return num( l->value + r->value );
            case '-': return num( l->value - r->value );
            case '*': return num( l->value * r->value );
            case '/': if ( r->value != 0 ) return num( l->value / r->value );
        }

    if ( r && r->value == 0 && b->op == '+' ) return b->left;
    if ( l && l->value == 0 && b->op == '+' ) return b->right;
    if ( r && r->value == 1 && b->op == '*' ) return b->left;
    if ( l && l->value == 1 && b->op == '*' ) return b->right;

    return n;
}

node_ptr simplify( const node_ptr &n ) { return bottom_up( n, simplify_rule ); }

int main() /* demo */
{
    /* let a = 2 * 3 in a * (b + 0) + (1 * c) */
    auto big = bin( '+', var( "b" ), var( "c" ) );
    auto tree = mk_let( "a", bin( '*', num( 2 ), num( 3 ) ),
                        bin( '+', bin( '*', var( "a" ), bin( '+', big, num( 0 ) ) ),
                                  bin( '*', num( 1 ), var( "c" ) ) ) );

    auto simple = simplify( tree );
    assert( show( simple ) == "(let a 6 (+ (* a (+ b c)) c))" );

    /* The old tree is unchanged – it is immutable, after all. */
    assert( show( tree ) == "(let a (* 2 3) (+ (* a (+ (+ b c) 0)) (* 1 c)))" );

    /* The sub-tree ‹b + c› did not change, so the new tree contains
     * the very same node (the same pointer) as the old one. */
    auto body = std::get< let >( simple->v ).body;
    auto prod = std::get< binary >( body->v ).left;
    assert( std::get< binary >( prod->v ).right == big );

    /* A tree with nothing to simplify comes back as the same node. */
    assert( simplify( big ) == big );

    /* Folding works bottom-up, so it cascades: */
    assert( show( simplify( bin( '*', bin( '+', num( 1 ), num( 2 ) ),
                                      bin( '-', num( 5 ), num( 1 ) ) ) ) ) == "12" );

    return 0;
}
