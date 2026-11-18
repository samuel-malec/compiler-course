#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/* In this demo, we extend ‹tiny› with statements, in a C-like
 * syntax:
 *
 *     program = { stmt } ;
 *     stmt    = 'var', ident, '=', expr, ';'
 *             | ident, '=', expr, ';'
 *             | 'print', expr, ';'
 *             | 'if', '(', expr, ')', stmt, [ 'else', stmt ]
 *             | 'while', '(', expr, ')', stmt
 *             | 'break', ';' | 'continue', ';'
 *             | '{', { stmt }, '}' ;
 *
 * Expressions are integer-valued, with the usual C operators. The
 * program ‘prints’ by appending numbers to an output vector. We will
 * see three new things: a parser for statements, a «scope stack» for
 * blocks, and «completions» for ‹break› and ‹continue›. */

struct error : std::runtime_error { using std::runtime_error::runtime_error; };

/* ─── lexer ──────────────────────────────────────────────────────── */

std::vector< std::string > lex( std::string_view s )
{
    std::vector< std::string > out;
    auto alnum = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_'; };

    for ( std::size_t i = 0; i < s.size(); )
    {
        std::size_t start = i++;
        char c = s[ start ];
        if ( c == ' ' || c == '\n' )
            continue;
        if ( alnum( c ) )
            while ( i < s.size() && alnum( s[ i ] ) ) ++ i;
        else if ( std::string_view( "=!<>&|" ).find( c ) != s.npos && i < s.size() &&
                  ( s[ i ] == '=' || ( ( c == '&' || c == '|' ) && s[ i ] == c ) ) )
            ++ i; /* two-character operators: ‹==›, ‹<=›, ‹&&›, … */
        out.emplace_back( s.substr( start, i - start ) );
    }

    out.emplace_back( "" );
    return out;
}

/* ─── abstract syntax ────────────────────────────────────────────── */

struct expr;
using expr_ptr = std::shared_ptr< const expr >;

struct expr
{
    std::string op;          /* "" for a number, "var" for a variable */
    long value = 0;
    std::string name = {};
    expr_ptr left = nullptr, right = nullptr;
};

struct stmt;
using stmt_ptr = std::shared_ptr< const stmt >;

/* Statements are a variant, as in Chapter 3. */

struct s_var    { std::string name; expr_ptr init; };
struct s_assign { std::string name; expr_ptr value; };
struct s_print  { expr_ptr value; };
struct s_if     { expr_ptr cond; stmt_ptr then, otherwise; };
struct s_while  { expr_ptr cond; stmt_ptr body; };
struct s_break  {};
struct s_continue {};
struct s_block  { std::vector< stmt_ptr > body; };

struct stmt
{
    std::variant< s_var, s_assign, s_print, s_if, s_while, s_break,
                  s_continue, s_block > v;
};

/* ─── parser ─────────────────────────────────────────────────────── */

struct parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    const std::string &peek() const { return toks[ pos ]; }
    std::string next() { return pos + 1 < toks.size() ? toks[ pos++ ] : toks[ pos ]; }
    bool accept( std::string_view t ) { if ( peek() != t ) return false; ++ pos; return true; }
    void expect( std::string_view t )
    {
        if ( !accept( t ) )
            throw error( "expected '" + std::string( t ) + "' but got '" + peek() + "'" );
    }

    static stmt_ptr make( auto s ) { return std::make_shared< stmt >( std::move( s ) ); }

    std::string ident()
    {
        std::string t = next();
        if ( t.empty() || t[ 0 ] < 'a' || t[ 0 ] > 'z' )
            throw error( "expected a name" );
        return t;
    }

    /* Statements are parsed by recursive descent: the first token
     * decides which kind of statement follows. Notice how the
     * ‘dangling else’ is resolved: the ‹else› belongs to the nearest
     * ‹if›, simply because the innermost ‹if› gets to look for it
     * first. */

    stmt_ptr statement()
    {
        if ( accept( "var" ) )
        {
            std::string name = ident();
            expect( "=" );
            auto init = expression();
            expect( ";" );
            return make( s_var{ name, init } );
        }

        if ( accept( "print" ) )
        {
            auto e = expression();
            expect( ";" );
            return make( s_print{ e } );
        }

        if ( accept( "if" ) )
        {
            expect( "(" );
            auto c = expression();
            expect( ")" );
            auto t = statement();
            stmt_ptr e = accept( "else" ) ? statement() : nullptr;
            return make( s_if{ c, t, e } );
        }

        if ( accept( "while" ) )
        {
            expect( "(" );
            auto c = expression();
            expect( ")" );
            return make( s_while{ c, statement() } );
        }

        if ( accept( "break" ) )    { expect( ";" ); return make( s_break{} ); }
        if ( accept( "continue" ) ) { expect( ";" ); return make( s_continue{} ); }

        if ( accept( "{" ) )
        {
            s_block b;
            while ( !accept( "}" ) )
            {
                if ( peek().empty() )
                    throw error( "unterminated block" );
                b.body.push_back( statement() );
            }
            return make( b );
        }

        std::string name = ident();
        expect( "=" );
        auto value = expression();
        expect( ";" );
        return make( s_assign{ name, value } );
    }

    /* Expressions: a Pratt parser, as in Chapter 2. */

    static int bp( const std::string &op )
    {
        static const std::map< std::string, int > table =
        {
            { "||", 1 }, { "&&", 2 }, { "==", 3 }, { "!=", 3 },
            { "<", 4 }, { "<=", 4 }, { ">", 4 }, { ">=", 4 },
            { "+", 5 }, { "-", 5 }, { "*", 6 }, { "/", 6 }, { "%", 6 },
        };
        auto it = table.find( op );
        return it == table.end() ? -1 : it->second;
    }

    expr_ptr expression( int min = 0 )
    {
        expr_ptr lhs = unary();
        while ( bp( peek() ) >= min && bp( peek() ) > 0 )
        {
            std::string op = next();
            lhs = std::make_shared< expr >( op, 0, "", lhs, expression( bp( op ) + 1 ) );
        }
        return lhs;
    }

    expr_ptr unary()
    {
        std::string t = next();
        if ( t == "-" || t == "!" )
            return std::make_shared< expr >( t, 0, "", unary() );
        if ( t == "(" )
        {
            auto e = expression();
            expect( ")" );
            return e;
        }
        if ( !t.empty() && t[ 0 ] >= '0' && t[ 0 ] <= '9' )
            return std::make_shared< expr >( "", std::stol( t ) );
        if ( !t.empty() && t[ 0 ] >= 'a' && t[ 0 ] <= 'z' )
            return std::make_shared< expr >( "var", 0, t );
        throw error( "expected an expression but got '" + t + "'" );
    }

    std::vector< stmt_ptr > program()
    {
        std::vector< stmt_ptr > out;
        while ( !peek().empty() )
            out.push_back( statement() );
        return out;
    }
};

/* ─── interpreter ────────────────────────────────────────────────── */

/* The result of executing a statement: either it finished normally,
 * or it wants to break out of / continue the innermost loop. */

enum class completion { normal, brk, cont };

template< typename... fs > struct overloaded : fs... { using fs::operator()...; };

struct interpreter
{
    /* The «scope stack»: one map per active block. Since there are no
     * functions (yet), the innermost scope is always the last one, and
     * lookup searches from the top down. */
    std::vector< std::map< std::string, long > > scopes{ {} };
    std::vector< long > output = {};

    long *find( const std::string &name )
    {
        for ( auto it = scopes.rbegin(); it != scopes.rend(); ++ it )
            if ( auto v = it->find( name ); v != it->end() )
                return &v->second;
        return nullptr;
    }

    long eval( const expr &e )
    {
        if ( e.op.empty() )
            return e.value;
        if ( e.op == "var" )
        {
            if ( long *v = find( e.name ) )
                return *v;
            throw error( "undeclared variable " + e.name );
        }
        if ( !e.right )
            return e.op == "-" ? -eval( *e.left ) : !eval( *e.left );

        /* ‹&&› and ‹||› short-circuit, as in C */
        if ( e.op == "&&" ) return eval( *e.left ) && eval( *e.right );
        if ( e.op == "||" ) return eval( *e.left ) || eval( *e.right );

        long a = eval( *e.left ), b = eval( *e.right );
        if ( ( e.op == "/" || e.op == "%" ) && b == 0 )
            throw error( "division by zero" );

        if ( e.op == "+" ) return a + b;
        if ( e.op == "-" ) return a - b;
        if ( e.op == "*" ) return a * b;
        if ( e.op == "/" ) return a / b;
        if ( e.op == "%" ) return a % b;
        if ( e.op == "<" ) return a < b;
        if ( e.op == "<=" ) return a <= b;
        if ( e.op == ">" ) return a > b;
        if ( e.op == ">=" ) return a >= b;
        if ( e.op == "==" ) return a == b;
        return a != b;
    }

    completion exec( const stmt &s )
    {
        return std::visit( overloaded
        {
            [ & ]( const s_var &d )
            {
                long v = eval( *d.init );
                if ( !scopes.back().emplace( d.name, v ).second )
                    throw error( "redeclared variable " + d.name );
                return completion::normal;
            },
            [ & ]( const s_assign &a )
            {
                long v = eval( *a.value );
                long *target = find( a.name );
                if ( !target )
                    throw error( "undeclared variable " + a.name );
                *target = v;
                return completion::normal;
            },
            [ & ]( const s_print &p )
            {
                output.push_back( eval( *p.value ) );
                return completion::normal;
            },
            [ & ]( const s_if &i )
            {
                if ( eval( *i.cond ) )
                    return exec( *i.then );
                return i.otherwise ? exec( *i.otherwise ) : completion::normal;
            },
            /* The loop is where ‹break› and ‹continue› are «consumed»:
             * a ‹brk› completion ends the loop (normally!), a ‹cont›
             * just goes on with the next iteration. */
            [ & ]( const s_while &w )
            {
                while ( eval( *w.cond ) )
                    if ( exec( *w.body ) == completion::brk )
                        break;
                return completion::normal;
            },
            []( const s_break & ) { return completion::brk; },
            []( const s_continue & ) { return completion::cont; },
            /* A block pushes a new scope, executes its statements, and
             * pops the scope again. If one of the statements completes
             * abnormally, the rest of the block is skipped and the
             * completion is passed on to the enclosing statement. Note
             * that the scope must be popped in either case. */
            [ & ]( const s_block &b )
            {
                scopes.emplace_back();
                completion c = completion::normal;
                for ( const auto &st : b.body )
                    if ( ( c = exec( *st ) ) != completion::normal )
                        break;
                scopes.pop_back();
                return c;
            }
        }, s.v );
    }
};

std::vector< long > run( std::string_view src )
{
    parser p{ lex( src ) };
    interpreter i;
    for ( const auto &s : p.program() )
        if ( i.exec( *s ) != completion::normal )
            throw error( "break or continue outside of a loop" );
    return i.output;
}

std::string error_of( std::string_view src )
{
    try { run( src ); }
    catch ( const error &e ) { return e.what(); }
    return "";
}

int main() /* demo */
{
    using v = std::vector< long >;

    assert(( run( "var x = 1; while (x < 100) x = x * 2; print x;" ) == v{ 128 } ));

    assert(( run( "var i = 0;"
                  "while (1) {"
                  "  i = i + 1;"
                  "  if (i % 2 == 0) continue;"
                  "  if (i > 7) break;"
                  "  print i;"
                  "}" ) == v{ 1, 3, 5, 7 } ));

    /* blocks and shadowing */
    assert(( run( "var x = 1; { var x = 2; print x; x = 3; print x; } print x;" ) ==
             v{ 2, 3, 1 } ));
    assert(( run( "var x = 1; { x = 5; var y = x; print y; } print x;" ) == v{ 5, 5 } ));

    /* dangling else: belongs to the inner if */
    assert(( run( "if (1) if (0) print 1; else print 2;" ) == v{ 2 } ));

    /* nested loops: break only leaves the inner one */
    assert(( run( "var i = 0; while (i < 3) { var j = 0;"
                  "  while (1) { if (j == i) break; j = j + 1; }"
                  "  print j; i = i + 1; }" ) == v{ 0, 1, 2 } ));

    assert( error_of( "print y;" ) == "undeclared variable y" );
    assert( error_of( "{ var y = 1; } y = 2;" ) == "undeclared variable y" );
    assert( error_of( "var x = 1; var x = 2;" ) == "redeclared variable x" );
    assert( error_of( "break;" ) == "break or continue outside of a loop" );
    assert( error_of( "print 1" ) == "expected ';' but got ''" );
    assert( error_of( "if (1) { print 1;" ) == "unterminated block" );

    return 0;
}
