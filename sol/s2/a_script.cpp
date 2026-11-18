#include <cassert>
#include <string>
#include <string_view>
#include <map>
#include <memory>
#include <set>
#include <variant>
#include <vector>

/* In this assignment, you will implement a complete interpreter for a
 * small dynamically typed scripting language with C-like syntax,
 * first-class functions and arrays – roughly the kind of language
 * that JavaScript or Lox are. It brings together everything from
 * blocks 1 and 2: a lexer, a parser for statements and expressions,
 * scopes, closures, recursion and non-local control flow.
 *
 * ── Syntax ─────────────────────────────────────────────────────────
 *
 *     program = { decl } ;
 *     decl    = 'fun', ident, '(', [ params ], ')', block
 *             | stmt ;
 *     params  = ident, { ',', ident } ;
 *     stmt    = 'var', ident, '=', expr, ';'
 *             | 'if', '(', expr, ')', stmt, [ 'else', stmt ]
 *             | 'while', '(', expr, ')', stmt
 *             | 'return', [ expr ], ';'
 *             | 'break', ';' | 'continue', ';'
 *             | 'print', expr, ';'
 *             | block
 *             | expr, [ '=', expr ], ';' ;
 *     block   = '{', { decl }, '}' ;
 *
 *     expr    = and, { '||', and } ;
 *     and     = eq, { '&&', eq } ;
 *     eq      = cmp, [ ( '==' | '!=' ), cmp ] ;
 *     cmp     = sum, [ ( '<' | '<=' | '>' | '>=' ), sum ] ;
 *     sum     = term, { ( '+' | '-' ), term } ;
 *     term    = unary, { ( '*' | '/' | '%' ), unary } ;
 *     unary   = ( '-' | '!' ), unary | postfix ;
 *     postfix = primary, { '(', [ args ], ')' | '[', expr, ']' } ;
 *     args    = expr, { ',', expr } ;
 *     primary = integer | string | 'true' | 'false' | 'nil' | ident
 *             | '(', expr, ')'
 *             | '[', [ args ], ']'
 *             | 'fun', '(', [ params ], ')', block ;
 *
 * Identifiers are letters, digits and ‹_›, starting with a letter or
 * ‹_›, other than the keywords (‹fun›, ‹var›, ‹if›, ‹else›, ‹while›,
 * ‹return›, ‹break›, ‹continue›, ‹print›, ‹true›, ‹false›, ‹nil›).
 * Integers are sequences of digits. Strings are enclosed in ‹"›, with
 * the escapes ‹\n›, ‹\"› and ‹\\›. Comments start with ‹//› and extend to
 * the end of the line. In an assignment ‹e₁ = e₂;›, ‹e₁› must be a
 * variable or an indexing expression ‹a[i]› (this is checked by the
 * parser). ‹==› and ‹!=› and the comparisons are non-associative.
 *
 * ── Semantics ──────────────────────────────────────────────────────
 *
 * Values are ‹nil›, integers (‹long›), booleans, strings, arrays and
 * functions. Arrays and functions are «references»: assigning an
 * array to another variable does not copy it, and changes through one
 * variable are visible through the other.
 *
 *  • Scoping is lexical; a block opens a new scope; ‹var› declares a
 *    variable in the current scope (it is an error to declare the same
 *    name twice in one scope); ‹fun name(…) {…}› declares ‹name› in the
 *    current scope «before» the function is created, so it can call
 *    itself. Functions are closures: they capture the scope in which
 *    they were created (by reference – they see later assignments).
 *    Assignment changes the nearest variable of that name.
 *  • ‹+› adds two integers or concatenates two strings; ‹- * / %› work on
 *    integers (‹/› and ‹%› truncate, as in C++); unary ‹-› negates an
 *    integer.
 *  • ‹==› and ‹!=› work on any two values: two values are equal if they
 *    are both ‹nil›, or are integers, booleans or strings with the same
 *    value, or are the «same» array or function (identity). The
 *    comparisons ‹< <= > >=› work on two integers or two strings.
 *  • ‹false› and ‹nil› are «falsy», everything else is «truthy» (also ‹0›
 *    and ‹""›). ‹!›, ‹&&› and ‹||› work on truthiness and give a boolean;
 *    ‹&&› and ‹||› do not evaluate the right operand if not needed. The
 *    conditions of ‹if› and ‹while› use truthiness, too.
 *  • ‹a[i]› indexes an array (or a string, giving a one-character
 *    string), with ‹i› an integer from ‹0› to ‹len - 1›.
 *  • A call evaluates the callee, then the arguments from left to
 *    right, and runs the body with the parameters bound in a new scope
 *    (whose parent is the captured scope). ‹return› ends the call; a
 *    function which ends without ‹return› (or with a bare ‹return;›)
 *    returns ‹nil›. ‹break› and ‹continue› refer to the innermost
 *    enclosing ‹while› in the same function; ‹return› outside of a
 *    function, and ‹break› or ‹continue› outside of a loop, are syntax
 *    errors (check them when parsing).
 *  • Built-in functions (ordinary values, bound in the global scope,
 *    which a program may shadow): ‹len(x)› – the length of a string or
 *    an array; ‹push(a, v)› – append ‹v› to the array ‹a›, returns ‹nil›;
 *    ‹str(v)› – the string which ‹print› would print for ‹v›.
 *  • ‹print e;› writes the value followed by a newline: integers in
 *    decimal, ‹true› / ‹false›, ‹nil›, strings as they are, functions as
 *    ‹<fun>›, arrays as ‹[e₁, e₂, …]› where the elements are written the
 *    same way, except that strings «inside» arrays are written in double
 *    quotes (without escaping anything).
 *
 * ── Interface ──────────────────────────────────────────────────────
 *
 * Write a function ‹run( program )›, which returns the output of the
 * program. If the program is syntactically invalid, the result is
 * just ‹syntax error\n› (nothing is executed). If a run-time error
 * happens, the output so far is followed by ‹runtime error: MSG\n›,
 * where ‹MSG› is one of: ‹undefined variable X› (reading or assigning
 * a variable which does not exist), ‹redefined variable X›, ‹type
 * error› (an operation on values of the wrong types, including calling
 * a built-in with arguments of wrong types), ‹division by zero›,
 * ‹index out of range›, ‹not a function›, ‹wrong number of arguments›.
 *
 * Deep recursion does not need to be supported beyond a few thousand
 * levels, and memory may be freed only at the end of ‹run› (see the
 * note on arenas in ‹06/00_intro.txt›). */

// std::string run( std::string_view program );

/* ─── lexer ──────────────────────────────────────────────────────── */

struct syntax_error {};

enum class tk { num, str, ident, kw, sym, end };
struct token { tk kind; std::string text; };

const std::set< std::string > keywords =
{
    "fun", "var", "if", "else", "while", "return", "break", "continue",
    "print", "true", "false", "nil"
};

std::vector< token > lex( std::string_view s )
{
    auto alpha = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_'; };
    auto digit = []( char c ) { return c >= '0' && c <= '9'; };
    std::vector< token > out;

    for ( std::size_t i = 0; i < s.size(); )
    {
        char c = s[ i ];
        if ( c == ' ' || c == '\n' || c == '\t' || c == '\r' ) { ++ i; continue; }
        if ( s.substr( i, 2 ) == "//" )
        {
            while ( i < s.size() && s[ i ] != '\n' ) ++ i;
            continue;
        }

        std::size_t start = i;
        if ( alpha( c ) )
        {
            while ( i < s.size() && ( alpha( s[ i ] ) || digit( s[ i ] ) ) ) ++ i;
            std::string w( s.substr( start, i - start ) );
            out.push_back( { keywords.contains( w ) ? tk::kw : tk::ident, w } );
        }
        else if ( digit( c ) )
        {
            while ( i < s.size() && digit( s[ i ] ) ) ++ i;
            out.push_back( { tk::num, std::string( s.substr( start, i - start ) ) } );
        }
        else if ( c == '"' )
        {
            std::string text;
            for ( ++ i; ; )
            {
                if ( i >= s.size() ) throw syntax_error();
                char d = s[ i++ ];
                if ( d == '"' ) break;
                if ( d == '\\' )
                {
                    if ( i >= s.size() ) throw syntax_error();
                    char e = s[ i++ ];
                    if ( e == 'n' ) text += '\n';
                    else if ( e == '"' || e == '\\' ) text += e;
                    else throw syntax_error();
                }
                else
                    text += d;
            }
            out.push_back( { tk::str, text } );
        }
        else
        {
            std::string two( s.substr( i, 2 ) );
            if ( two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "&&" || two == "||" )
                i += 2, out.push_back( { tk::sym, two } );
            else if ( std::string_view( "+-*/%<>=!(){}[],;" ).find( c ) != std::string_view::npos )
                ++ i, out.push_back( { tk::sym, std::string( 1, c ) } );
            else
                throw syntax_error();
        }
    }

    out.push_back( { tk::end, "" } );
    return out;
}

/* ─── syntax tree ────────────────────────────────────────────────── */

struct node;
using node_ptr = std::unique_ptr< node >;

enum class nk
{
    /* expressions */
    num, str, boolean, nil, var, array, func, unary, binary, call, index,
    /* statements */
    vardecl, fundecl, assign, exprstmt, print, block, ifs, whiles, ret, brk, cont
};

struct node
{
    nk k;
    std::string text = {};                     /* name, operator, string literal */
    long num = 0;
    std::vector< node_ptr > kids = {};
    std::vector< std::string > params = {};
};

node_ptr make( nk k, std::string text = "", std::vector< node_ptr > kids = {} )
{
    auto n = std::make_unique< node >( k, std::move( text ) );
    n->kids = std::move( kids );
    return n;
}

/* ─── parser ─────────────────────────────────────────────────────── */

struct parser
{
    std::vector< token > toks;
    std::size_t pos = 0;
    int loops = 0, functions = 0; /* for checking break / continue / return */

    const token &peek() const { return toks[ pos ]; }
    bool is( std::string_view t ) const
    {
        return ( peek().kind == tk::sym || peek().kind == tk::kw ) && peek().text == t;
    }
    bool accept( std::string_view t ) { if ( !is( t ) ) return false; ++ pos; return true; }
    void expect( std::string_view t ) { if ( !accept( t ) ) throw syntax_error(); }

    std::string ident()
    {
        if ( peek().kind != tk::ident ) throw syntax_error();
        return toks[ pos++ ].text;
    }

    std::vector< node_ptr > program()
    {
        std::vector< node_ptr > out;
        while ( peek().kind != tk::end )
            out.push_back( decl() );
        return out;
    }

    /* the parameters and body of a function; a new function resets the
     * loop depth (break cannot cross a function boundary) */
    node_ptr function( nk kind, std::string name )
    {
        auto f = make( kind, name );
        expect( "(" );
        if ( !accept( ")" ) )
        {
            do f->params.push_back( ident() ); while ( accept( "," ) );
            expect( ")" );
        }
        int saved = loops;
        loops = 0, ++ functions;
        f->kids.push_back( block() );
        loops = saved, -- functions;
        return f;
    }

    node_ptr decl()
    {
        if ( is( "fun" ) && toks[ pos + 1 ].kind == tk::ident )
        {
            ++ pos;
            std::string name = ident();
            return function( nk::fundecl, name );
        }
        return stmt();
    }

    node_ptr block()
    {
        expect( "{" );
        auto b = make( nk::block );
        while ( !accept( "}" ) )
        {
            if ( peek().kind == tk::end ) throw syntax_error();
            b->kids.push_back( decl() );
        }
        return b;
    }

    node_ptr stmt()
    {
        if ( accept( "var" ) )
        {
            std::string name = ident();
            expect( "=" );
            std::vector< node_ptr > k;
            k.push_back( expr() );
            expect( ";" );
            return make( nk::vardecl, name, std::move( k ) );
        }
        if ( accept( "if" ) )
        {
            expect( "(" );
            std::vector< node_ptr > k;
            k.push_back( expr() );
            expect( ")" );
            k.push_back( stmt() );
            if ( accept( "else" ) ) k.push_back( stmt() );
            return make( nk::ifs, "", std::move( k ) );
        }
        if ( accept( "while" ) )
        {
            expect( "(" );
            std::vector< node_ptr > k;
            k.push_back( expr() );
            expect( ")" );
            ++ loops;
            k.push_back( stmt() );
            -- loops;
            return make( nk::whiles, "", std::move( k ) );
        }
        if ( accept( "return" ) )
        {
            if ( !functions ) throw syntax_error();
            std::vector< node_ptr > k;
            if ( !is( ";" ) ) k.push_back( expr() );
            expect( ";" );
            return make( nk::ret, "", std::move( k ) );
        }
        if ( is( "break" ) || is( "continue" ) )
        {
            bool brk = toks[ pos++ ].text == "break";
            if ( !loops ) throw syntax_error();
            expect( ";" );
            return make( brk ? nk::brk : nk::cont );
        }
        if ( accept( "print" ) )
        {
            std::vector< node_ptr > k;
            k.push_back( expr() );
            expect( ";" );
            return make( nk::print, "", std::move( k ) );
        }
        if ( is( "{" ) )
            return block();

        std::vector< node_ptr > k;
        k.push_back( expr() );
        if ( accept( "=" ) )
        {
            if ( k[ 0 ]->k != nk::var && k[ 0 ]->k != nk::index ) throw syntax_error();
            k.push_back( expr() );
            expect( ";" );
            return make( nk::assign, "", std::move( k ) );
        }
        expect( ";" );
        return make( nk::exprstmt, "", std::move( k ) );
    }

    node_ptr binary( std::string op, node_ptr a, node_ptr b )
    {
        std::vector< node_ptr > k;
        k.push_back( std::move( a ) );
        k.push_back( std::move( b ) );
        return make( nk::binary, op, std::move( k ) );
    }

    node_ptr expr()
    {
        auto l = conj();
        while ( accept( "||" ) ) l = binary( "||", std::move( l ), conj() );
        return l;
    }

    node_ptr conj()
    {
        auto l = eq();
        while ( accept( "&&" ) ) l = binary( "&&", std::move( l ), eq() );
        return l;
    }

    node_ptr eq()
    {
        auto l = cmp();
        for ( auto op : { "==", "!=" } )
            if ( accept( op ) )
            {
                l = binary( op, std::move( l ), cmp() );
                if ( is( "==" ) || is( "!=" ) ) throw syntax_error();
                break;
            }
        return l;
    }

    node_ptr cmp()
    {
        auto l = sum();
        for ( auto op : { "<", "<=", ">", ">=" } )
            if ( accept( op ) )
            {
                l = binary( op, std::move( l ), sum() );
                if ( is( "<" ) || is( "<=" ) || is( ">" ) || is( ">=" ) ) throw syntax_error();
                break;
            }
        return l;
    }

    node_ptr sum()
    {
        auto l = term();
        while ( is( "+" ) || is( "-" ) )
        {
            std::string op = toks[ pos++ ].text;
            l = binary( op, std::move( l ), term() );
        }
        return l;
    }

    node_ptr term()
    {
        auto l = unary();
        while ( is( "*" ) || is( "/" ) || is( "%" ) )
        {
            std::string op = toks[ pos++ ].text;
            l = binary( op, std::move( l ), unary() );
        }
        return l;
    }

    node_ptr unary()
    {
        if ( is( "-" ) || is( "!" ) )
        {
            std::string op = toks[ pos++ ].text;
            std::vector< node_ptr > k;
            k.push_back( unary() );
            return make( nk::unary, op, std::move( k ) );
        }
        return postfix();
    }

    std::vector< node_ptr > args( std::string_view close )
    {
        std::vector< node_ptr > out;
        if ( accept( close ) ) return out;
        do out.push_back( expr() ); while ( accept( "," ) );
        expect( close );
        return out;
    }

    node_ptr postfix()
    {
        auto e = primary();
        while ( true )
            if ( accept( "(" ) )
            {
                auto a = args( ")" );
                a.insert( a.begin(), std::move( e ) );
                e = make( nk::call, "", std::move( a ) );
            }
            else if ( accept( "[" ) )
            {
                std::vector< node_ptr > k;
                k.push_back( std::move( e ) );
                k.push_back( expr() );
                expect( "]" );
                e = make( nk::index, "", std::move( k ) );
            }
            else
                return e;
    }

    node_ptr primary()
    {
        const token t = peek();
        ++ pos;
        switch ( t.kind )
        {
            case tk::num: { auto n = make( nk::num ); n->num = std::stol( t.text ); return n; }
            case tk::str: return make( nk::str, t.text );
            case tk::ident: return make( nk::var, t.text );
            case tk::end: throw syntax_error();
            default: break;
        }
        if ( t.text == "true" || t.text == "false" ) { auto n = make( nk::boolean ); n->num = t.text == "true"; return n; }
        if ( t.text == "nil" ) return make( nk::nil );
        if ( t.text == "(" ) { auto e = expr(); expect( ")" ); return e; }
        if ( t.text == "[" ) return make( nk::array, "", args( "]" ) );
        if ( t.text == "fun" ) return function( nk::func, "" );
        throw syntax_error();
    }
};

/* ─── values ─────────────────────────────────────────────────────── */

struct array;
struct closure;
enum class builtin { len, push, str };
struct nil_t { bool operator==( const nil_t & ) const = default; };

using value = std::variant< nil_t, long, bool, std::string, array *, const closure *, builtin >;

struct array { std::vector< value > items; };
struct scope { std::map< std::string, value > vars; scope *parent; };
struct closure { const node *fn; scope *env; };

struct runtime_error { std::string msg; };
struct return_signal { value v; };
struct break_signal {};
struct continue_signal {};

std::string show( const value &v, bool quoted = false )
{
    if ( std::holds_alternative< nil_t >( v ) ) return "nil";
    if ( auto i = std::get_if< long >( &v ) ) return std::to_string( *i );
    if ( auto b = std::get_if< bool >( &v ) ) return *b ? "true" : "false";
    if ( auto s = std::get_if< std::string >( &v ) ) return quoted ? "\"" + *s + "\"" : *s;
    if ( auto a = std::get_if< array * >( &v ) )
    {
        std::string out = "[";
        for ( std::size_t i = 0; i < ( *a )->items.size(); ++ i )
            out += ( i ? ", " : "" ) + show( ( *a )->items[ i ], true );
        return out + "]";
    }
    return "<fun>";
}

bool truthy( const value &v )
{
    if ( std::holds_alternative< nil_t >( v ) ) return false;
    if ( auto b = std::get_if< bool >( &v ) ) return *b;
    return true;
}

/* ─── interpreter ────────────────────────────────────────────────── */

struct interpreter
{
    std::vector< std::unique_ptr< scope > > scopes;
    std::vector< std::unique_ptr< array > > arrays;
    std::vector< std::unique_ptr< closure > > closures;
    std::string out;

    scope *new_scope( scope *parent )
    {
        scopes.push_back( std::make_unique< scope >( scope{ {}, parent } ) );
        return scopes.back().get();
    }

    [[noreturn]] static void fail( std::string msg ) { throw runtime_error{ std::move( msg ) }; }
    [[noreturn]] static void type_error() { fail( "type error" ); }

    static long as_int( const value &v )
    {
        if ( auto i = std::get_if< long >( &v ) ) return *i;
        type_error();
    }

    value *find( scope *s, const std::string &name )
    {
        for ( ; s; s = s->parent )
            if ( auto it = s->vars.find( name ); it != s->vars.end() )
                return &it->second;
        fail( "undefined variable " + name );
    }

    void define( scope *s, const std::string &name, value v )
    {
        if ( !s->vars.emplace( name, std::move( v ) ).second )
            fail( "redefined variable " + name );
    }

    static bool equal( const value &a, const value &b )
    {
        if ( a.index() != b.index() ) return false;
        return a == b; /* pointers compare by identity */
    }

    value call( const value &f, std::vector< value > args )
    {
        if ( auto b = std::get_if< builtin >( &f ) )
        {
            if ( *b == builtin::len )
            {
                if ( args.size() != 1 ) fail( "wrong number of arguments" );
                if ( auto s = std::get_if< std::string >( &args[ 0 ] ) ) return long( s->size() );
                if ( auto a = std::get_if< array * >( &args[ 0 ] ) ) return long( ( *a )->items.size() );
                type_error();
            }
            if ( *b == builtin::push )
            {
                if ( args.size() != 2 ) fail( "wrong number of arguments" );
                auto a = std::get_if< array * >( &args[ 0 ] );
                if ( !a ) type_error();
                ( *a )->items.push_back( args[ 1 ] );
                return nil_t{};
            }
            if ( args.size() != 1 ) fail( "wrong number of arguments" );
            return show( args[ 0 ] );
        }

        auto c = std::get_if< const closure * >( &f );
        if ( !c ) fail( "not a function" );
        const node &fn = *( *c )->fn;
        if ( fn.params.size() != args.size() ) fail( "wrong number of arguments" );

        scope *s = new_scope( ( *c )->env );
        for ( std::size_t i = 0; i < args.size(); ++ i )
            define( s, fn.params[ i ], args[ i ] );

        try
        {
            exec_block( *fn.kids[ 0 ], s );
        }
        catch ( return_signal &r ) { return r.v; }
        return nil_t{};
    }

    value eval( const node &e, scope *env )
    {
        switch ( e.k )
        {
            case nk::num: return e.num;
            case nk::str: return e.text;
            case nk::boolean: return bool( e.num );
            case nk::nil: return nil_t{};
            case nk::var: return *find( env, e.text );
            case nk::array:
            {
                arrays.push_back( std::make_unique< array >() );
                array *a = arrays.back().get();
                for ( const auto &k : e.kids ) a->items.push_back( eval( *k, env ) );
                return a;
            }
            case nk::func:
                closures.push_back( std::make_unique< closure >( &e, env ) );
                return closures.back().get();
            case nk::unary:
            {
                value v = eval( *e.kids[ 0 ], env );
                if ( e.text == "!" ) return !truthy( v );
                return -as_int( v );
            }
            case nk::call:
            {
                value f = eval( *e.kids[ 0 ], env );
                std::vector< value > args;
                for ( std::size_t i = 1; i < e.kids.size(); ++ i )
                    args.push_back( eval( *e.kids[ i ], env ) );
                return call( f, std::move( args ) );
            }
            case nk::index:
            {
                value base = eval( *e.kids[ 0 ], env );
                long i = as_int( eval( *e.kids[ 1 ], env ) );
                if ( auto a = std::get_if< array * >( &base ) )
                {
                    if ( i < 0 || i >= long( ( *a )->items.size() ) ) fail( "index out of range" );
                    return ( *a )->items[ i ];
                }
                if ( auto s = std::get_if< std::string >( &base ) )
                {
                    if ( i < 0 || i >= long( s->size() ) ) fail( "index out of range" );
                    return std::string( 1, ( *s )[ i ] );
                }
                type_error();
            }
            case nk::binary: break;
            default: type_error();
        }

        const std::string &op = e.text;
        if ( op == "&&" ) return truthy( eval( *e.kids[ 0 ], env ) ) && truthy( eval( *e.kids[ 1 ], env ) );
        if ( op == "||" ) return truthy( eval( *e.kids[ 0 ], env ) ) || truthy( eval( *e.kids[ 1 ], env ) );

        value a = eval( *e.kids[ 0 ], env ), b = eval( *e.kids[ 1 ], env );

        if ( op == "==" ) return equal( a, b );
        if ( op == "!=" ) return !equal( a, b );

        auto sa = std::get_if< std::string >( &a ), sb = std::get_if< std::string >( &b );
        if ( sa && sb )
        {
            if ( op == "+" ) return *sa + *sb;
            if ( op == "<" ) return *sa < *sb;
            if ( op == "<=" ) return *sa <= *sb;
            if ( op == ">" ) return *sa > *sb;
            if ( op == ">=" ) return *sa >= *sb;
            type_error();
        }

        long x = as_int( a ), y = as_int( b );
        if ( op == "+" ) return x + y;
        if ( op == "-" ) return x - y;
        if ( op == "*" ) return x * y;
        if ( op == "<" ) return x < y;
        if ( op == "<=" ) return x <= y;
        if ( op == ">" ) return x > y;
        if ( op == ">=" ) return x >= y;
        if ( y == 0 ) fail( "division by zero" );
        return op == "/" ? x / y : x % y;
    }

    void exec_block( const node &b, scope *s )
    {
        for ( const auto &k : b.kids )
            exec( *k, s );
    }

    void exec( const node &s, scope *env )
    {
        switch ( s.k )
        {
            case nk::vardecl: { value v = eval( *s.kids[ 0 ], env ); define( env, s.text, v ); break; }
            case nk::fundecl:
                define( env, s.text, nil_t{} );
                closures.push_back( std::make_unique< closure >( &s, env ) );
                env->vars[ s.text ] = closures.back().get();
                break;
            case nk::assign:
            {
                const node &target = *s.kids[ 0 ];
                if ( target.k == nk::var )
                {
                    value v = eval( *s.kids[ 1 ], env );
                    *find( env, target.text ) = v;
                }
                else
                {
                    value base = eval( *target.kids[ 0 ], env );
                    long i = as_int( eval( *target.kids[ 1 ], env ) );
                    value v = eval( *s.kids[ 1 ], env );
                    auto a = std::get_if< array * >( &base );
                    if ( !a ) type_error();
                    if ( i < 0 || i >= long( ( *a )->items.size() ) ) fail( "index out of range" );
                    ( *a )->items[ i ] = v;
                }
                break;
            }
            case nk::exprstmt: eval( *s.kids[ 0 ], env ); break;
            case nk::print: out += show( eval( *s.kids[ 0 ], env ) ) + "\n"; break;
            case nk::block: exec_block( s, new_scope( env ) ); break;
            case nk::ifs:
                if ( truthy( eval( *s.kids[ 0 ], env ) ) )
                    exec( *s.kids[ 1 ], env );
                else if ( s.kids.size() == 3 )
                    exec( *s.kids[ 2 ], env );
                break;
            case nk::whiles:
                while ( truthy( eval( *s.kids[ 0 ], env ) ) )
                {
                    try { exec( *s.kids[ 1 ], env ); }
                    catch ( const break_signal & ) { break; }
                    catch ( const continue_signal & ) {}
                }
                break;
            case nk::ret:
                throw return_signal{ s.kids.empty() ? value( nil_t{} ) : eval( *s.kids[ 0 ], env ) };
            case nk::brk: throw break_signal();
            case nk::cont: throw continue_signal();
            default: break;
        }
    }
};

std::string run( std::string_view program )
{
    std::vector< node_ptr > prog;
    try
    {
        parser p{ lex( program ) };
        prog = p.program();
    }
    catch ( const syntax_error & ) { return "syntax error\n"; }

    interpreter i;
    scope *global = i.new_scope( nullptr );
    global->vars[ "len" ] = builtin::len;
    global->vars[ "push" ] = builtin::push;
    global->vars[ "str" ] = builtin::str;

    try
    {
        for ( const auto &s : prog )
            i.exec( *s, global );
    }
    catch ( const runtime_error &e ) { i.out += "runtime error: " + e.msg + "\n"; }
    return i.out;
}

int main()
{
    assert( run( "print 1 + 2 * 3;" ) == "7\n" );
    assert( run( "print \"a\" + \"b\"; print true; print nil; print [1, \"x\", [true]];" ) ==
            "ab\ntrue\nnil\n[1, \"x\", [true]]\n" );
    assert( run( "var x = 10; { var x = 20; print x; x = 30; } print x;" ) == "20\n10\n" );

    assert( run( "fun fact(n) { if (n <= 1) return 1; return n * fact(n - 1); }\n"
                 "print fact(10);" ) == "3628800\n" );

    assert( run( "fun counter() { var n = 0; return fun() { n = n + 1; return n; }; }\n"
                 "var a = counter(); var b = counter();\n"
                 "a(); a(); print a(); print b();" ) == "3\n1\n" );

    assert( run( "var xs = [];\n"
                 "var i = 0;\n"
                 "while (true) { i = i + 1; if (i % 2 == 0) continue; if (i > 7) break; push(xs, i); }\n"
                 "print xs; print len(xs);" ) == "[1, 3, 5, 7]\n4\n" );

    assert( run( "fun map(f, a) { var r = []; var i = 0;\n"
                 "  while (i < len(a)) { push(r, f(a[i])); i = i + 1; } return r; }\n"
                 "print map(fun(x) { return x * x; }, [1, 2, 3]);" ) == "[1, 4, 9]\n" );

    assert( run( "var a = [1, 2]; var b = a; b[0] = 5; print a; print a == b; print [1] == [1];" ) ==
            "[5, 2]\ntrue\nfalse\n" );
    assert( run( "print nil && x; print 0 && nil; print nil || \"y\"; print !0;" ) == "false\nfalse\ntrue\nfalse\n" );
    assert( run( "var s = \"hello\"; print s[1] + str(len(s)); // comment\n" ) == "e5\n" );
    assert( run( "fun f() { } print f(); print f; print str([\"q\"]);" ) == "nil\n<fun>\n[\"q\"]\n" );

    assert( run( "print 1; print x;" ) == "1\nruntime error: undefined variable x\n" );
    assert( run( "print 1 + true;" ) == "runtime error: type error\n" );
    assert( run( "print [1][1];" ) == "runtime error: index out of range\n" );
    assert( run( "print 7 / (2 - 2);" ) == "runtime error: division by zero\n" );
    assert( run( "var f = 5; f();" ) == "runtime error: not a function\n" );
    assert( run( "fun f(a) { } f(1, 2);" ) == "runtime error: wrong number of arguments\n" );
    assert( run( "var a = 1; var a = 2;" ) == "runtime error: redefined variable a\n" );

    assert( run( "print 1" ) == "syntax error\n" );
    assert( run( "print 1; 1 + 2 = 3;" ) == "syntax error\n" );
    assert( run( "break;" ) == "syntax error\n" );
    assert( run( "fun f() { while (true) { fun g() { break; } } }" ) == "syntax error\n" );
    assert( run( "return 1;" ) == "syntax error\n" );
    assert( run( "print 1 < 2 < 3;" ) == "syntax error\n" );

    return 0;
}
