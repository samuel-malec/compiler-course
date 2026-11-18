#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/* In this demo, we will represent the same AST as in ‹d1_hierarchy›,
 * but using ‹std::variant›. Each kind of node is a plain ‹struct›
 * with no methods at all, and a node is a variant of all of them.
 * Since the variant needs to contain the structs (not pointers to
 * them), but the structs need to refer to nodes, we have to break the
 * cycle somewhere: the structs hold their children through pointers
 * to the (incomplete) type ‹node›, which wraps the variant. */

struct node;
using node_ptr = std::unique_ptr< node >;

struct number   { long value; };
struct variable { std::string name; };
struct binary   { char op; node_ptr left, right; };
struct let      { std::string name; node_ptr value, body; };

struct node
{
    std::variant< number, variable, binary, let > v;
};

/* Builders, as before. Thanks to aggregate initialization, they are
 * one-liners. */

node_ptr num( long v ) { return std::make_unique< node >( number{ v } ); }
node_ptr var( std::string n ) { return std::make_unique< node >( variable{ n } ); }

node_ptr bin( char op, node_ptr l, node_ptr r )
{
    return std::make_unique< node >( binary{ op, std::move( l ), std::move( r ) } );
}

node_ptr mk_let( std::string n, node_ptr v, node_ptr b )
{
    return std::make_unique< node >( let{ n, std::move( v ), std::move( b ) } );
}

/* Operations are now ordinary functions, and ‹std::visit› takes care
 * of the dispatch. The most convenient way to give ‹std::visit› one
 * case per alternative is the ‹overloaded› trick: a struct which
 * inherits the call operators of several lambdas. */

template< typename... fs > struct overloaded : fs... { using fs::operator()...; };

std::string show( const node &n )
{
    return std::visit( overloaded
    {
        []( const number &x ) { return std::to_string( x.value ); },
        []( const variable &x ) { return x.name; },
        []( const binary &x )
        {
            return std::string( "(" ) + x.op + " " + show( *x.left ) + " " +
                   show( *x.right ) + ")";
        },
        []( const let &x )
        {
            return "(let " + x.name + " " + show( *x.value ) + " " +
                   show( *x.body ) + ")";
        }
    }, n.v );
}

/* Structural equality: two trees are equal if they have the same
 * shape and the same values at the leaves. The default ‹==› on
 * ‹std::unique_ptr› compares the «pointers», not the trees, so we
 * need to write it ourselves. A neat way to compare two variants is
 * to visit «both» at once: ‹std::visit› with two variants calls the
 * function with every combination of alternatives, and the generic
 * fallback case handles all the mismatched pairs. */

bool equal( const node &a, const node &b )
{
    return std::visit( overloaded
    {
        []( const number &x, const number &y ) { return x.value == y.value; },
        []( const variable &x, const variable &y ) { return x.name == y.name; },
        []( const binary &x, const binary &y )
        {
            return x.op == y.op && equal( *x.left, *y.left ) &&
                   equal( *x.right, *y.right );
        },
        []( const let &x, const let &y )
        {
            return x.name == y.name && equal( *x.value, *y.value ) &&
                   equal( *x.body, *y.body );
        },
        []( const auto &, const auto & ) { return false; }
    }, a.v, b.v );
}

/* Analyses which only care about some kinds of nodes can use a
 * generic lambda for the rest – but they still need to recurse into
 * children. Here we use ‹std::get_if› instead of ‹std::visit›, which
 * is sometimes more convenient. */

int depth( const node &n )
{
    if ( auto b = std::get_if< binary >( &n.v ) )
        return 1 + std::max( depth( *b->left ), depth( *b->right ) );
    if ( auto l = std::get_if< let >( &n.v ) )
        return 1 + std::max( depth( *l->value ), depth( *l->body ) );
    return 1;
}

/* Finally, a parser which builds the tree. This is a Pratt parser,
 * as in ‹02/d2_pratt›, except that it returns nodes instead of
 * strings. The grammar:
 *
 *     expr = 'let', ident, '=', expr, 'in', expr
 *          | expr, ( '+' | '-' | '*' | '/' ), expr
 *          | number | ident | '(', expr, ')' ;
 *
 * with the usual precedence and associativity. To keep things short,
 * the lexer is built into the parser. */

struct parser
{
    std::string_view s;
    std::size_t pos = 0;

    static bool is_alpha( char c ) { return c >= 'a' && c <= 'z'; }
    static bool is_digit( char c ) { return c >= '0' && c <= '9'; }

    void ws() { while ( pos < s.size() && s[ pos ] == ' ' ) ++ pos; }

    /* Read the next token without consuming it. */
    std::string_view peek()
    {
        ws();
        std::size_t end = pos;
        if ( end < s.size() && is_alpha( s[ end ] ) )
            while ( end < s.size() && is_alpha( s[ end ] ) ) ++ end;
        else if ( end < s.size() && is_digit( s[ end ] ) )
            while ( end < s.size() && is_digit( s[ end ] ) ) ++ end;
        else if ( end < s.size() )
            ++ end;
        return s.substr( pos, end - pos );
    }

    std::string_view next() { auto t = peek(); pos += t.size(); return t; }

    void expect( std::string_view t )
    {
        if ( next() != t )
            throw std::runtime_error( "expected " + std::string( t ) );
    }

    static int bp( std::string_view op )
    {
        if ( op == "+" || op == "-" ) return 1;
        if ( op == "*" || op == "/" ) return 3;
        return -1;
    }

    node_ptr expr( int min_bp = 0 )
    {
        node_ptr lhs = atom();

        while ( bp( peek() ) >= min_bp )
        {
            char op = next()[ 0 ];
            node_ptr rhs = expr( bp( std::string_view( &op, 1 ) ) + 1 );
            lhs = bin( op, std::move( lhs ), std::move( rhs ) );
        }

        return lhs;
    }

    node_ptr atom()
    {
        auto t = next();

        if ( t == "(" )
        {
            auto e = expr();
            expect( ")" );
            return e;
        }

        if ( t == "let" )
        {
            std::string name( next() );
            expect( "=" );
            auto value = expr();
            expect( "in" );
            return mk_let( name, std::move( value ), expr() );
        }

        if ( !t.empty() && is_digit( t[ 0 ] ) )
            return num( std::stol( std::string( t ) ) );
        if ( !t.empty() && is_alpha( t[ 0 ] ) && t != "in" )
            return var( std::string( t ) );

        throw std::runtime_error( "unexpected token" );
    }
};

node_ptr parse( std::string_view s )
{
    parser p{ s };
    auto e = p.expr();
    if ( !p.peek().empty() )
        throw std::runtime_error( "trailing input" );
    return e;
}

int main() /* demo */
{
    auto tree = mk_let( "x", num( 2 ),
                        bin( '*', var( "x" ), bin( '+', var( "y" ), num( 1 ) ) ) );

    assert( show( *tree ) == "(let x 2 (* x (+ y 1)))" );
    assert( depth( *tree ) == 4 );

    auto parsed = parse( "let x = 2 in x * (y + 1)" );
    assert( equal( *tree, *parsed ) );
    assert( !equal( *tree, *parse( "let x = 2 in x * y + 1" ) ) );

    assert( show( *parse( "1 - 2 - 3" ) ) == "(- (- 1 2) 3)" );
    assert( show( *parse( "a * (b - c) / d" ) ) == "(/ (* a (- b c)) d)" );

    return 0;
}
