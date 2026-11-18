#include <cassert>
#include <memory>
#include <string>
#include <vector>

/* In this demo, we will represent the abstract syntax of ‹tiny›
 * expressions as a class hierarchy. To keep the demo short, we will
 * only have 4 kinds of nodes: numbers, variables, binary operators
 * and ‹let›. Each kind is a class derived from the abstract base
 * class ‹node›. Children are owned through ‹std::unique_ptr›. */

struct visitor; /* see below */

struct node
{
    virtual ~node() = default;

    /* The traditional object-oriented way to add an operation to the
     * tree is a virtual method. We will add one, ‹show›, which
     * prints the tree as an S-expression. */
    virtual std::string show() const = 0;

    /* The ‘hook’ for the visitor pattern, explained further down. */
    virtual void accept( visitor &v ) const = 0;
};

using node_ptr = std::unique_ptr< node >;

struct number;
struct variable;
struct binary;
struct let;

/* The «visitor» pattern solves this problem: we want to add new
 * operations on the tree without adding new virtual methods to all
 * the classes each time. A visitor is a class with one method per
 * kind of node. Each node class implements ‹accept› by calling the
 * appropriate method of the visitor – this is the «only» place where
 * the node classes need to know about operations. The mechanism is
 * also known as «double dispatch»: the first virtual call
 * (‹accept›) selects the kind of node, the second (‹visit›) selects
 * the operation. */

struct visitor
{
    virtual void visit( const number & ) = 0;
    virtual void visit( const variable & ) = 0;
    virtual void visit( const binary & ) = 0;
    virtual void visit( const let & ) = 0;
    virtual ~visitor() = default;
};

/* Now the node classes themselves. Notice that the ‹show› method is
 * repeated in each, while ‹accept› is always the same boilerplate.
 */

struct number : node
{
    long value;
    explicit number( long v ) : value( v ) {}

    std::string show() const override { return std::to_string( value ); }
    void accept( visitor &v ) const override { v.visit( *this ); }
};

struct variable : node
{
    std::string name;
    explicit variable( std::string n ) : name( std::move( n ) ) {}

    std::string show() const override { return name; }
    void accept( visitor &v ) const override { v.visit( *this ); }
};

struct binary : node
{
    char op;
    node_ptr left, right;

    binary( char op, node_ptr l, node_ptr r )
        : op( op ), left( std::move( l ) ), right( std::move( r ) ) {}

    std::string show() const override
    {
        return std::string( "(" ) + op + " " + left->show() + " " +
               right->show() + ")";
    }

    void accept( visitor &v ) const override { v.visit( *this ); }
};

struct let : node
{
    std::string name;
    node_ptr value, body;

    let( std::string n, node_ptr v, node_ptr b )
        : name( std::move( n ) ), value( std::move( v ) ), body( std::move( b ) ) {}

    std::string show() const override
    {
        return "(let " + name + " " + value->show() + " " + body->show() + ")";
    }

    void accept( visitor &v ) const override { v.visit( *this ); }
};

/* Some helpers to build trees with less typing. */

node_ptr num( long v ) { return std::make_unique< number >( v ); }
node_ptr var( std::string n ) { return std::make_unique< variable >( n ); }
node_ptr bin( char op, node_ptr l, node_ptr r )
{
    return std::make_unique< binary >( op, std::move( l ), std::move( r ) );
}
node_ptr mk_let( std::string n, node_ptr v, node_ptr b )
{
    return std::make_unique< let >( n, std::move( v ), std::move( b ) );
}

/* A new operation, implemented as a visitor: count the nodes of each
 * kind. Since ‹visit› returns nothing, the visitor accumulates its
 * result in its own member variables. Recursion into children goes
 * through ‹accept›, so that the right ‹visit› gets called for them. */

struct counter : visitor
{
    int numbers = 0, variables = 0, binaries = 0, lets = 0;

    void visit( const number & ) override { ++ numbers; }
    void visit( const variable & ) override { ++ variables; }

    void visit( const binary &b ) override
    {
        ++ binaries;
        b.left->accept( *this );
        b.right->accept( *this );
    }

    void visit( const let &l ) override
    {
        ++ lets;
        l.value->accept( *this );
        l.body->accept( *this );
    }
};

/* Another one: collect the names of all variables that are «used»
 * (i.e. appear as a ‹variable› node), in order of appearance. */

struct uses : visitor
{
    std::vector< std::string > names;

    void visit( const number & ) override {}
    void visit( const variable &v ) override { names.push_back( v.name ); }
    void visit( const binary &b ) override
    {
        b.left->accept( *this );
        b.right->accept( *this );
    }
    void visit( const let &l ) override
    {
        l.value->accept( *this );
        l.body->accept( *this );
    }
};

/* Notice how much boilerplate even these tiny operations needed –
 * especially the traversal of children, which is the same in both
 * visitors. Compare this with ‹d2_variant›. */

int main() /* demo */
{
    /* let x = 2 in x * (y + 1) */
    node_ptr tree = mk_let( "x", num( 2 ),
                            bin( '*', var( "x" ),
                                      bin( '+', var( "y" ), num( 1 ) ) ) );

    assert( tree->show() == "(let x 2 (* x (+ y 1)))" );

    counter c;
    tree->accept( c );
    assert( c.numbers == 2 );
    assert( c.variables == 2 );
    assert( c.binaries == 2 );
    assert( c.lets == 1 );

    uses u;
    tree->accept( u );
    assert(( u.names == std::vector< std::string >{ "x", "y" } ));

    return 0;
}
