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

/* Write a function ‹vars› which returns the set of names of all
 * variables which are «used» in the tree (that is, which appear in a
 * ‹variable› node). The name bound by a ‹let› only counts if it is
 * also used. */

// std::set< std::string > vars( const node_ptr &n );

void collect( const node_ptr &n, std::set< std::string > &out )
{
    std::visit( overloaded
    {
        [ & ]( const variable &v ) { out.insert( v.name ); },
        [ & ]( const binary &b ) { collect( b.left, out ); collect( b.right, out ); },
        [ & ]( const let &l ) { collect( l.value, out ); collect( l.body, out ); },
        []( const number & ) {}
    }, n->v );
}

std::set< std::string > vars( const node_ptr &n )
{
    std::set< std::string > out;
    collect( n, out );
    return out;
}

int main()
{
    using s = std::set< std::string >;

    assert(( vars( sx( "1" ) ) == s{} ));
    assert(( vars( sx( "x" ) ) == s{ "x" } ));
    assert(( vars( sx( "(+ x (* y x))" ) ) == s{ "x", "y" } ));
    assert(( vars( sx( "(let x 1 y)" ) ) == s{ "y" } ));
    assert(( vars( sx( "(let x a (let y b (+ x z)))" ) ) == s{ "a", "b", "x", "z" } ));

    return 0;
}
