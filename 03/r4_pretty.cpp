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

/* A «pretty printer» formats a tree so that it fits into a given
 * line width, breaking it into lines only where necessary. Write a
 * function ‹pretty( tree, width )› which prints a tree in the
 * S-expression syntax of ‹show›, using the following rules:
 *
 *  • if the whole node, printed flat (as by ‹show›), fits on the
 *    current line, print it flat,
 *  • otherwise, if it is a leaf (a number or a variable), print it
 *    flat anyway (there is no way to break it),
 *  • otherwise, print the ‘head’ of the node – ‹(+› for a binary
 *    operator (and similarly for the others), ‹(let NAME› for a
 *    ‹let› – and then each of its two children on a line of its own,
 *    indented by 2 more spaces than the node itself; the closing ‹)›
 *    is printed right after the last child.
 *
 * A node «fits» on the current line if ⟦i + s + t ≤ width⟧, where
 * ⟦i⟧ is the column at which it starts (its indentation), ⟦s⟧ is the
 * length of its flat form, and ⟦t⟧ is the number of closing
 * parentheses which will be printed right after it (on the same
 * line), because it is the last child of one or more enclosing
 * nodes. Lines are separated by ‹\n›, and there is no newline at the
 * end of the output. The tree itself starts at column 0. */

// std::string pretty( const node_ptr &n, int width );

int main()
{
    auto t = sx( "(let x (* 1 2) (+ x (* x x)))" );

    assert( pretty( sx( "x" ), 0 ) == "x" );
    assert( pretty( t, 80 ) == show( t ) );
    assert( pretty( t, 29 ) == show( t ) );

    assert( pretty( t, 28 ) == "(let x\n"
                               "  (* 1 2)\n"
                               "  (+ x (* x x)))" );

    assert( pretty( t, 16 ) == pretty( t, 28 ) );

    assert( pretty( t, 15 ) == "(let x\n"
                               "  (* 1 2)\n"
                               "  (+\n"
                               "    x\n"
                               "    (* x x)))" );

    assert( pretty( t, 1 ) == "(let x\n"
                              "  (*\n"
                              "    1\n"
                              "    2)\n"
                              "  (+\n"
                              "    x\n"
                              "    (*\n"
                              "      x\n"
                              "      x)))" );

    auto deep = sx( "(- (+ (+ a b) c) d)" );
    assert( pretty( deep, 12 ) == "(-\n"
                                  "  (+\n"
                                  "    (+ a b)\n"
                                  "    c)\n"
                                  "  d)" );

    return 0;
}
