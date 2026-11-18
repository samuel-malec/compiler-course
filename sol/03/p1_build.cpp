#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
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

/* Write a parser which builds an AST for the following grammar:
 *
 *     expr  = 'let', ident, '=', expr, 'in', expr | sum ;
 *     sum   = prod, { ( '+' | '-' ), prod } ;
 *     prod  = unary, { ( '*' | '/' ), unary } ;
 *     unary = '-', unary | atom ;
 *     atom  = number | ident | '(', expr, ')' ;
 *
 * where ‹number› is a non-empty sequence of decimal digits and
 * ‹ident› is a non-empty sequence of lowercase letters which is not
 * one of the keywords ‹let› and ‹in›. Spaces may appear between
 * tokens. The binary operators are left-associative.
 *
 * Our AST has no node for unary minus. Instead, the parser should
 * «desugar» it: ‹-e› becomes ‹0 - e› (that is, ‹(- 0 e)›). This is a
 * common trick to keep the AST (and hence all later phases) small.
 *
 * The function ‹parse› returns the root of the tree, or ‹nullptr› if
 * the input does not match the grammar. */

// node_ptr parse( std::string_view src );

/* Syntax errors are signalled by an exception, which ‹parse› turns
 * into a ‹nullptr›. */

struct syntax_error {};

struct ast_parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    bool accept( std::string_view t )
    {
        if ( peek() != t ) return false;
        ++ pos;
        return true;
    }

    void expect( std::string_view t ) { if ( !accept( t ) ) throw syntax_error(); }

    static bool is_ident( const std::string &t )
    {
        return !t.empty() && t[ 0 ] >= 'a' && t[ 0 ] <= 'z' && t != "let" && t != "in";
    }

    node_ptr expr()
    {
        if ( !accept( "let" ) )
            return sum();

        if ( !is_ident( peek() ) )
            throw syntax_error();
        std::string name = toks[ pos++ ];
        expect( "=" );
        node_ptr value = expr();
        expect( "in" );
        return mk_let( name, value, expr() );
    }

    node_ptr sum()
    {
        node_ptr l = prod();
        while ( peek() == "+" || peek() == "-" )
        {
            char op = toks[ pos++ ][ 0 ];
            l = bin( op, l, prod() );
        }
        return l;
    }

    node_ptr prod()
    {
        node_ptr l = unary();
        while ( peek() == "*" || peek() == "/" )
        {
            char op = toks[ pos++ ][ 0 ];
            l = bin( op, l, unary() );
        }
        return l;
    }

    node_ptr unary()
    {
        if ( accept( "-" ) )
            return bin( '-', num( 0 ), unary() );
        return atom();
    }

    node_ptr atom()
    {
        if ( accept( "(" ) )
        {
            node_ptr e = expr();
            expect( ")" );
            return e;
        }

        const std::string &t = peek();
        if ( !t.empty() && t[ 0 ] >= '0' && t[ 0 ] <= '9' )
            return num( std::stol( toks[ pos++ ] ) );
        if ( is_ident( t ) )
            return var( toks[ pos++ ] );

        throw syntax_error();
    }
};

node_ptr parse( std::string_view src )
{
    auto cls = []( char c ) { return c >= 'a' && c <= 'z' ? 1 : c >= '0' && c <= '9' ? 2 : 0; };
    std::vector< std::string > toks;

    for ( std::size_t i = 0; i < src.size(); )
    {
        std::size_t start = i++;
        char c = src[ start ];
        if ( c == ' ' )
            continue;
        if ( cls( c ) )
            while ( i < src.size() && cls( src[ i ] ) == cls( c ) ) ++ i;
        else if ( std::string_view( "+-*/()=" ).find( c ) == src.npos )
            return nullptr;
        toks.emplace_back( src.substr( start, i - start ) );
    }

    ast_parser p{ toks };
    try
    {
        node_ptr e = p.expr();
        return p.pos == toks.size() ? e : nullptr;
    }
    catch ( syntax_error ) { return nullptr; }
}

int main()
{
    auto p = []( std::string_view s ) { return show( parse( s ) ); };

    assert( p( "42" ) == "42" );
    assert( p( "x" ) == "x" );
    assert( p( "1 + 2 * 3" ) == "(+ 1 (* 2 3))" );
    assert( p( "(1 + 2) * 3" ) == "(* (+ 1 2) 3)" );
    assert( p( "a - b - c" ) == "(- (- a b) c)" );
    assert( p( "a / b / c" ) == "(/ (/ a b) c)" );
    assert( p( "-x" ) == "(- 0 x)" );
    assert( p( "--x * y" ) == "(* (- 0 (- 0 x)) y)" );
    assert( p( "let x = 5 in x * x" ) == "(let x 5 (* x x))" );
    assert( p( "let x = let y = 1 in y in x" ) == "(let x (let y 1 y) x)" );
    assert( p( "1 + (let x = 2 in x)" ) == "(+ 1 (let x 2 x))" );
    assert( p( "letx" ) == "letx" );
    assert( p( "  ( ( a ) )  " ) == "a" );

    assert( !parse( "" ) );
    assert( !parse( "1 +" ) );
    assert( !parse( "(1" ) );
    assert( !parse( "1 2" ) );
    assert( !parse( "let in = 1 in 2" ) );
    assert( !parse( "let x = 1" ) );
    assert( !parse( "let x 1 in 2" ) );
    assert( !parse( "1 + let x = 1 in x" ) );
    assert( !parse( "in" ) );
    assert( !parse( "X" ) );

    return 0;
}
