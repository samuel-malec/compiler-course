#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/* This demo is the review of block 1: a complete interpreter for
 * ‹tiny› (version 1), from source text to value. It consists of the
 * three phases we have studied so far – a lexer (Chapter 1), a Pratt
 * parser which builds an AST (Chapters 2 and 3) and a tree-walking
 * evaluator (this chapter). Each phase reports errors by throwing an
 * exception of its own type, and the function ‹run› at the end turns
 * the result (or the error) into a string.
 *
 * The language:
 *
 *     expr  = 'let', ident, '=', expr, 'in', expr
 *           | 'if', expr, 'then', expr, 'else', expr
 *           | expr, binop, expr
 *           | ( '-' | 'not' ), expr
 *           | integer | string | 'true' | 'false' | ident
 *           | '(', expr, ')' ;
 *     binop = 'or' | 'and' | '==' | '!=' | '<' | '<=' | '>' | '>='
 *           | '+' | '-' | '*' | '/' | '%' ;
 *
 * with precedence (from the loosest): ‹let› and ‹if›; ‹or›; ‹and›;
 * comparisons (non-associative); ‹+ -›; ‹* / %›; prefix ‹-› and ‹not›.
 * Values are integers, booleans and strings; ‹+› also concatenates
 * strings and ‹<› (and friends) also compare strings. */

struct error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/* ─── lexer ──────────────────────────────────────────────────────── */

enum class tok { ident, keyword, integer, string, op, end };

struct token
{
    tok kind;
    std::string text;
};

std::vector< token > lex( std::string_view s )
{
    static const std::string_view keywords[] =
        { "let", "in", "if", "then", "else", "true", "false", "and", "or", "not" };

    auto alpha = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_'; };
    auto digit = []( char c ) { return c >= '0' && c <= '9'; };

    std::vector< token > out;
    std::size_t i = 0;

    while ( i < s.size() )
    {
        std::size_t start = i;
        char c = s[ i++ ];

        if ( c == ' ' || c == '\n' || c == '\t' )
            continue;

        if ( c == '#' )
        {
            while ( i < s.size() && s[ i ] != '\n' ) ++ i;
            continue;
        }

        if ( alpha( c ) )
        {
            while ( i < s.size() && ( alpha( s[ i ] ) || digit( s[ i ] ) ) ) ++ i;
            std::string word( s.substr( start, i - start ) );
            bool kw = false;
            for ( auto k : keywords ) kw = kw || word == k;
            out.push_back( { kw ? tok::keyword : tok::ident, word } );
        }
        else if ( digit( c ) )
        {
            while ( i < s.size() && digit( s[ i ] ) ) ++ i;
            out.push_back( { tok::integer, std::string( s.substr( start, i - start ) ) } );
        }
        else if ( c == '"' )
        {
            auto end = s.find( '"', i );
            if ( end == s.npos )
                throw error( "lexical error: unterminated string" );
            out.push_back( { tok::string, std::string( s.substr( i, end - i ) ) } );
            i = end + 1;
        }
        else if ( std::string_view( "+-*/%()" ).find( c ) != s.npos )
            out.push_back( { tok::op, std::string( 1, c ) } );
        else if ( std::string_view( "=!<>" ).find( c ) != s.npos )
        {
            if ( i < s.size() && s[ i ] == '=' ) ++ i;
            std::string op( s.substr( start, i - start ) );
            if ( op == "!" ) /* only ‹!=› is valid */
                throw error( "lexical error: unexpected '!'" );
            out.push_back( { tok::op, op } );
        }
        else
            throw error( std::string( "lexical error: unexpected '" ) + c + "'" );
    }

    out.push_back( { tok::end, "" } );
    return out;
}

/* ─── abstract syntax ────────────────────────────────────────────── */

using value = std::variant< long, bool, std::string >;

struct node;
using node_ptr = std::shared_ptr< const node >;

struct literal  { value v; };
struct variable { std::string name; };
struct unary    { std::string op; node_ptr arg; };
struct binary   { std::string op; node_ptr left, right; };
struct let      { std::string name; node_ptr value, body; };
struct cond     { node_ptr test, then, otherwise; };

struct node
{
    std::variant< literal, variable, unary, binary, let, cond > v;
};

node_ptr make( auto n ) { return std::make_shared< node >( std::move( n ) ); }

/* ─── parser ─────────────────────────────────────────────────────── */

struct parser
{
    std::vector< token > toks;
    std::size_t pos = 0;

    const token &peek() const { return toks[ pos ]; }
    token next() { return toks[ pos < toks.size() - 1 ? pos++ : pos ]; }

    bool is( std::string_view text ) const
    {
        return peek().kind != tok::string && peek().text == text;
    }

    void expect( std::string_view text )
    {
        if ( !is( text ) )
            throw error( "syntax error: expected '" + std::string( text ) + "'" );
        next();
    }

    static int left_bp( const token &t )
    {
        static const std::map< std::string, int > bp =
        {
            { "or", 1 }, { "and", 3 },
            { "==", 5 }, { "!=", 5 }, { "<", 5 }, { "<=", 5 }, { ">", 5 }, { ">=", 5 },
            { "+", 7 }, { "-", 7 }, { "*", 9 }, { "/", 9 }, { "%", 9 },
        };
        if ( t.kind != tok::op && t.kind != tok::keyword )
            return -1;
        auto it = bp.find( t.text );
        return it == bp.end() ? -1 : it->second;
    }

    node_ptr expr( int min_bp = 0 )
    {
        node_ptr lhs = operand();
        bool after_cmp = false;

        while ( left_bp( peek() ) >= min_bp )
        {
            std::string op = next().text;
            int bp = left_bp( { tok::op, op } );

            if ( bp == 5 && after_cmp )
                throw error( "syntax error: comparisons do not associate" );
            after_cmp = bp == 5;

            lhs = make( binary{ op, lhs, expr( bp + 1 ) } );
        }

        return lhs;
    }

    node_ptr operand()
    {
        token t = next();

        switch ( t.kind )
        {
            case tok::integer: return make( literal{ std::stol( t.text ) } );
            case tok::string:  return make( literal{ t.text } );
            case tok::ident:   return make( variable{ t.text } );
            case tok::end:     throw error( "syntax error: unexpected end of input" );
            default: break;
        }

        if ( t.text == "true" || t.text == "false" )
            return make( literal{ t.text == "true" } );

        if ( t.text == "-" || t.text == "not" )
            return make( unary{ t.text, expr( 11 ) } );

        if ( t.text == "(" )
        {
            node_ptr e = expr();
            expect( ")" );
            return e;
        }

        if ( t.text == "let" )
        {
            token name = next();
            if ( name.kind != tok::ident )
                throw error( "syntax error: expected a variable name" );
            expect( "=" );
            node_ptr v = expr();
            expect( "in" );
            return make( let{ name.text, v, expr() } );
        }

        if ( t.text == "if" )
        {
            node_ptr c = expr();
            expect( "then" );
            node_ptr a = expr();
            expect( "else" );
            return make( cond{ c, a, expr() } );
        }

        throw error( "syntax error: unexpected '" + t.text + "'" );
    }
};

node_ptr parse( std::string_view src )
{
    parser p{ lex( src ) };
    node_ptr e = p.expr();
    if ( p.peek().kind != tok::end )
        throw error( "syntax error: unexpected '" + p.peek().text + "'" );
    return e;
}

/* ─── evaluator ──────────────────────────────────────────────────── */

struct frame;
using env = std::shared_ptr< const frame >;
struct frame { std::string name; value val; env parent; };

template< typename... fs > struct overloaded : fs... { using fs::operator()...; };

template< typename T >
const T &as( const value &v, const char *what )
{
    if ( auto p = std::get_if< T >( &v ) )
        return *p;
    throw error( std::string( "type error: expected " ) + what );
}

value eval( const node_ptr &n, const env &e );

value eval_binary( const binary &b, const env &e )
{
    if ( b.op == "and" )
        return as< bool >( eval( b.left, e ), "a boolean" ) &&
               as< bool >( eval( b.right, e ), "a boolean" );
    if ( b.op == "or" )
        return as< bool >( eval( b.left, e ), "a boolean" ) ||
               as< bool >( eval( b.right, e ), "a boolean" );

    value l = eval( b.left, e ), r = eval( b.right, e );

    if ( b.op == "==" ) return l == r;
    if ( b.op == "!=" ) return l != r;

    /* Comparisons and ‹+› work on two integers or two strings; the
     * comparison operators of ‹std::variant› do the right thing once
     * we know the alternatives are the same. */
    if ( b.op == "<" || b.op == "<=" || b.op == ">" || b.op == ">=" || b.op == "+" )
    {
        if ( l.index() != r.index() || std::holds_alternative< bool >( l ) )
            throw error( "type error: cannot apply " + b.op + " to these values" );

        if ( b.op == "<" )  return l < r;
        if ( b.op == "<=" ) return l <= r;
        if ( b.op == ">" )  return l > r;
        if ( b.op == ">=" ) return l >= r;

        if ( auto s = std::get_if< std::string >( &l ) )
            return *s + std::get< std::string >( r );
    }

    long x = as< long >( l, "an integer" ), y = as< long >( r, "an integer" );

    if ( b.op == "+" ) return x + y;
    if ( b.op == "-" ) return x - y;
    if ( b.op == "*" ) return x * y;
    if ( y == 0 )
        throw error( "run-time error: division by zero" );
    return b.op == "/" ? x / y : x % y;
}

value eval( const node_ptr &n, const env &e )
{
    return std::visit( overloaded
    {
        []( const literal &l ) { return l.v; },
        [ & ]( const variable &v ) -> value
        {
            for ( const frame *f = e.get(); f; f = f->parent.get() )
                if ( f->name == v.name )
                    return f->val;
            throw error( "run-time error: unbound variable " + v.name );
        },
        [ & ]( const unary &u ) -> value
        {
            value a = eval( u.arg, e );
            if ( u.op == "not" )
                return !as< bool >( a, "a boolean" );
            return -as< long >( a, "an integer" );
        },
        [ & ]( const binary &b ) { return eval_binary( b, e ); },
        [ & ]( const let &l )
        {
            value v = eval( l.value, e );
            return eval( l.body, std::make_shared< frame >( l.name, v, e ) );
        },
        [ & ]( const cond &c )
        {
            return as< bool >( eval( c.test, e ), "a boolean" )
                ? eval( c.then, e ) : eval( c.otherwise, e );
        }
    }, n->v );
}

/* ─── putting it together ────────────────────────────────────────── */

std::string show( const value &v )
{
    return std::visit( overloaded
    {
        []( long i ) { return std::to_string( i ); },
        []( bool b ) { return std::string( b ? "true" : "false" ); },
        []( const std::string &s ) { return "\"" + s + "\""; }
    }, v );
}

std::string run( std::string_view src )
{
    try
    {
        return show( eval( parse( src ), nullptr ) );
    }
    catch ( const error &e )
    {
        return e.what();
    }
}

int main() /* demo */
{
    assert( run( "1 + 2 * 3" ) == "7" );
    assert( run( "let x = 6 in let y = 7 in x * y" ) == "42" );
    assert( run( "if 2 < 3 then \"yes\" else \"no\"" ) == "\"yes\"" );
    assert( run( "\"ab\" + \"cd\"" ) == "\"abcd\"" );
    assert( run( "\"abc\" < \"abd\" and not (1 == 2)" ) == "true" );
    assert( run( "-(3 - 5) % 3" ) == "2" );
    assert( run( "# a comment\nlet answer = 42 in\n  answer / 2" ) == "21" );
    assert( run( "let x = 1 in (let x = 2 in x) + x" ) == "3" );
    assert( run( "false and 1 / 0 == 0" ) == "false" );
    assert( run( "1 + let x = 1 in x" ) == "2" );

    assert( run( "1 + $" ) == "lexical error: unexpected '$'" );
    assert( run( "\"abc" ) == "lexical error: unterminated string" );
    assert( run( "(1 + 2" ) == "syntax error: expected ')'" );
    assert( run( "1 < 2 < 3" ) == "syntax error: comparisons do not associate" );
    assert( run( "let 1 = 2 in 3" ) == "syntax error: expected a variable name" );
    assert( run( "1 +" ) == "syntax error: unexpected end of input" );
    assert( run( "1 2" ) == "syntax error: unexpected '2'" );
    assert( run( "1 - true" ) == "type error: expected an integer" );
    assert( run( "1 + true" ) == "type error: cannot apply + to these values" );
    assert( run( "\"a\" + 1" ) == "type error: cannot apply + to these values" );
    assert( run( "if 1 then 2 else 3" ) == "type error: expected a boolean" );
    assert( run( "x" ) == "run-time error: unbound variable x" );
    assert( run( "7 % (2 - 2)" ) == "run-time error: division by zero" );

    return 0;
}
