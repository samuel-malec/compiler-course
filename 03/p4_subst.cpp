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

/* «Substitution» replaces a variable with an expression: substituting
 * ‹3› for ‹x› in ‹x + y› gives ‹3 + y›. It is the basis of many
 * transformations, most notably «inlining». In the presence of
 * binders like ‹let›, substitution is subtle:
 *
 *  • only «free» occurrences of the variable are replaced: in
 *    ‹let x = 1 in x›, the ‹x› in the body refers to the ‹let›, not to
 *    the variable we are substituting for,
 *  • a free variable of the substituted expression must not be
 *    «captured» by a ‹let› in the target: substituting ‹y› for ‹x› in
 *    ‹let y = 1 in x + y› must not give ‹let y = 1 in y + y› (where
 *    both ‹y›s now refer to the ‹let›); instead, the bound variable
 *    must first be «renamed»: ‹let y' = 1 in y + y'›.
 *
 * Write a function ‹subst( t, x, e )› which computes the result of
 * substituting ‹e› for the free occurrences of ‹x› in ‹t›, as
 * follows:
 *
 *  • a variable ‹x› becomes ‹e›; other variables and numbers are
 *    unchanged; for a binary operator, substitute in both operands,
 *  • for ‹let y = V in B›, always substitute in ‹V›; then:
 *    ◦ if ‹y› is ‹x›, leave ‹B› unchanged (‹x› is shadowed),
 *    ◦ if ‹y› is free in ‹e› «and» ‹x› is free in ‹B›, rename ‹y›: the
 *      new name is ‹y› followed by one or more ‹'› characters – as
 *      few as possible, such that the new name is free neither in
 *      ‹e› nor in ‹B›; replace ‹y› by the new name in ‹B› (itself a
 *      substitution!) and then substitute ‹e› for ‹x› in the result,
 *    ◦ otherwise, just substitute in ‹B›. */

// node_ptr subst( const node_ptr &t, const std::string &x, const node_ptr &e );

int main()
{
    auto s = []( std::string_view t, std::string x, std::string_view e )
    {
        return show( subst( sx( t ), x, sx( e ) ) );
    };

    assert( s( "x", "x", "3" ) == "3" );
    assert( s( "y", "x", "3" ) == "y" );
    assert( s( "(+ x (* x y))", "x", "(- a 1)" ) == "(+ (- a 1) (* (- a 1) y))" );
    assert( s( "(let x 1 x)", "x", "3" ) == "(let x 1 x)" );
    assert( s( "(let x x x)", "x", "3" ) == "(let x 3 x)" );
    assert( s( "(let y 1 (+ x y))", "x", "z" ) == "(let y 1 (+ z y))" );
    assert( s( "(let y 1 (+ x y))", "x", "y" ) == "(let y' 1 (+ y y'))" );
    assert( s( "(let y 1 (+ x (+ y y')))", "x", "y" ) ==
            "(let y'' 1 (+ y (+ y'' y')))" );
    assert( s( "(let y 1 (+ x y))", "x", "(+ y y')" ) ==
            "(let y'' 1 (+ (+ y y') y''))" );
    assert( s( "(let y 1 y)", "x", "y" ) == "(let y 1 y)" );
    assert( s( "(let y x (+ x y))", "x", "(* y 2)" ) ==
            "(let y' (* y 2) (+ (* y 2) y'))" );
    assert( s( "(let y 1 (let z 2 (+ x (+ y z))))", "x", "(+ y z)" ) ==
            "(let y' 1 (let z' 2 (+ (+ y z) (+ y' z'))))" );
    assert( s( "(let y 1 (let y 2 (+ x y)))", "x", "y" ) ==
            "(let y' 1 (let y' 2 (+ y y')))" );

    return 0;
}
