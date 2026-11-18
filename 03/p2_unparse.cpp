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

/* Write a function ‹unparse›, which turns a tree back into the
 * concrete syntax of ‹p1_build›, using «as few parentheses as
 * possible» while keeping the meaning: parsing the output (with the
 * parser from ‹p1_build›) must give back the same tree.
 *
 *  • A binary operator is printed as ‹A op B›, with one space on each
 *    side of the operator. The operators ‹*› and ‹/› bind tighter
 *    than ‹+› and ‹-›, and all of them are left-associative. Hence,
 *    the «left» operand needs parentheses if its operator binds less
 *    tightly than the parent's, while the «right» operand needs them
 *    if its operator binds less tightly «or equally»: the tree
 *    ‹(- a (- b c))› is ‹a - (b - c)›, not ‹a - b - c›.
 *  • A ‹let› is printed as ‹let x = V in B›. Since ‹let› is not
 *    allowed as an operand of a binary operator, it needs parentheses
 *    whenever it is one; it never needs them as the value or the body
 *    of another ‹let›.
 *  • Parentheses are written as ‹(…)›, without any extra spaces.
 *
 * You can assume that all numbers in the tree are non-negative. */

// std::string unparse( const node_ptr &n );

int main()
{
    auto u = []( std::string_view s ) { return unparse( sx( s ) ); };

    assert( u( "42" ) == "42" );
    assert( u( "(+ 1 (* 2 3))" ) == "1 + 2 * 3" );
    assert( u( "(* (+ 1 2) 3)" ) == "(1 + 2) * 3" );
    assert( u( "(- (- a b) c)" ) == "a - b - c" );
    assert( u( "(- a (- b c))" ) == "a - (b - c)" );
    assert( u( "(+ a (+ b c))" ) == "a + (b + c)" );
    assert( u( "(- a (+ b c))" ) == "a - (b + c)" );
    assert( u( "(+ (- a b) c)" ) == "a - b + c" );
    assert( u( "(/ a (* b c))" ) == "a / (b * c)" );
    assert( u( "(* a (/ b c))" ) == "a * (b / c)" );
    assert( u( "(- (* a b) (/ c d))" ) == "a * b - c / d" );
    assert( u( "(* (- a b) (+ c d))" ) == "(a - b) * (c + d)" );
    assert( u( "(let x 1 (+ x 2))" ) == "let x = 1 in x + 2" );
    assert( u( "(+ (let x 1 x) 2)" ) == "(let x = 1 in x) + 2" );
    assert( u( "(+ 2 (let x 1 x))" ) == "2 + (let x = 1 in x)" );
    assert( u( "(let x (let y 1 y) (let z x z))" ) ==
            "let x = let y = 1 in y in let z = x in z" );

    return 0;
}
