#include <cassert>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/* This demo is the review of block 2: ‹tiny›, version 2 – a small
 * functional language in the style of ML, with first-class functions,
 * recursion, closures and Hindley–Milner type inference. Programs are
 * checked before they run, and well-typed programs cannot go wrong.
 *
 *     expr  = 'let', [ 'rec' ], ident, { ident }, '=', expr, 'in', expr
 *           | 'fn', ident, { ident }, '=>', expr
 *           | 'if', expr, 'then', expr, 'else', expr
 *           | expr, binop, expr
 *           | app ;
 *     app   = atom, { atom } ;          (* function application *)
 *     atom  = integer | 'true' | 'false' | ident | '(', expr, ')' ;
 *     binop = '||' | '&&' | '==' | '!=' | '<' | '<=' | '>' | '>='
 *           | '+' | '-' | '*' | '/' ;
 *
 * Functions are «curried»: ‹fn x y => e› is ‹fn x => fn y => e›, and
 * ‹f a b› is ‹(f a) b›; ‹let f x = e in …› is ‹let f = fn x => e in …›.
 * Application binds tighter than any operator, so ‹f x + 1› is
 * ‹(f x) + 1›. The phases are the same as in ‹04/d2_tiny›, plus type
 * inference between parsing and evaluation. */

struct error : std::runtime_error { using std::runtime_error::runtime_error; };

/* ─── lexer ──────────────────────────────────────────────────────── */

std::vector< std::string > lex( std::string_view s )
{
    auto alnum = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
                                       ( c >= '0' && c <= '9' ) || c == '_'; };
    std::vector< std::string > out;

    for ( std::size_t i = 0; i < s.size(); )
    {
        std::size_t start = i++;
        char c = s[ start ];
        if ( c == ' ' || c == '\n' )
            continue;
        if ( alnum( c ) )
            while ( i < s.size() && alnum( s[ i ] ) ) ++ i;
        else
            for ( std::string_view two : { "=>", "==", "!=", "<=", ">=", "&&", "||" } )
                if ( s.substr( start, 2 ) == two )
                    i = start + 2;
        if ( i == start + 1 && std::string_view( "()+-*/<>=" ).find( c ) == s.npos && !alnum( c ) )
            throw error( std::string( "syntax error: unexpected '" ) + c + "'" );
        out.emplace_back( s.substr( start, i - start ) );
    }

    out.emplace_back( "" );
    return out;
}

/* ─── abstract syntax ────────────────────────────────────────────── */

/* After desugaring, every function has exactly one parameter and
 * every application exactly one argument. */

struct expr;
using expr_ptr = std::shared_ptr< const expr >;

struct expr
{
    enum kind { num, boolean, var, binop, cond, fn, app, let, letrec } k;
    long value = 0;
    std::string name = {};                  /* variable, operator or bound name */
    expr_ptr a = nullptr, b = nullptr, c = nullptr;
};

expr_ptr make( expr::kind k, std::string name = "", expr_ptr a = nullptr,
               expr_ptr b = nullptr, expr_ptr c = nullptr, long v = 0 )
{
    return std::make_shared< expr >( k, v, name, a, b, c );
}

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
            throw error( "syntax error: expected '" + std::string( t ) + "'" );
    }

    static bool keyword( const std::string &t )
    {
        for ( auto k : { "let", "rec", "in", "fn", "if", "then", "else", "true", "false" } )
            if ( t == k ) return true;
        return false;
    }

    static bool is_ident( const std::string &t )
    {
        return !t.empty() && ( ( t[ 0 ] >= 'a' && t[ 0 ] <= 'z' ) || t[ 0 ] == '_' ) && !keyword( t );
    }

    std::string ident()
    {
        if ( !is_ident( peek() ) )
            throw error( "syntax error: expected a name" );
        return next();
    }

    /* ‹fn x y => e› becomes ‹fn x => fn y => e› */
    expr_ptr curry( const std::vector< std::string > &params, std::size_t i, expr_ptr body )
    {
        if ( i == params.size() )
            return body;
        return make( expr::fn, params[ i ], curry( params, i + 1, body ) );
    }

    static int bp( const std::string &op )
    {
        static const std::map< std::string, int > table =
        {
            { "||", 1 }, { "&&", 2 }, { "==", 3 }, { "!=", 3 }, { "<", 4 }, { "<=", 4 },
            { ">", 4 }, { ">=", 4 }, { "+", 5 }, { "-", 5 }, { "*", 6 }, { "/", 6 },
        };
        auto it = table.find( op );
        return it == table.end() ? 0 : it->second;
    }

    expr_ptr expression( int min = 1 )
    {
        if ( accept( "let" ) )
        {
            bool rec = accept( "rec" );
            std::string name = ident();
            std::vector< std::string > params;
            while ( is_ident( peek() ) )
                params.push_back( next() );
            expect( "=" );
            auto value = curry( params, 0, expression() );
            expect( "in" );
            return make( rec ? expr::letrec : expr::let, name, value, expression() );
        }

        if ( accept( "fn" ) )
        {
            std::vector< std::string > params{ ident() };
            while ( is_ident( peek() ) )
                params.push_back( next() );
            expect( "=>" );
            return curry( params, 0, expression() );
        }

        if ( accept( "if" ) )
        {
            auto c = expression();
            expect( "then" );
            auto t = expression();
            expect( "else" );
            return make( expr::cond, "", c, t, expression() );
        }

        expr_ptr lhs = application();
        while ( bp( peek() ) >= min )
        {
            std::string op = next();
            lhs = make( expr::binop, op, lhs, expression( bp( op ) + 1 ) );
        }
        return lhs;
    }

    bool starts_atom() const
    {
        const std::string &t = peek();
        return t == "(" || t == "true" || t == "false" || is_ident( t ) ||
               ( !t.empty() && t[ 0 ] >= '0' && t[ 0 ] <= '9' );
    }

    expr_ptr application()
    {
        expr_ptr f = atom();
        while ( starts_atom() )
            f = make( expr::app, "", f, atom() );
        return f;
    }

    expr_ptr atom()
    {
        std::string t = next();
        if ( t == "(" )
        {
            auto e = expression();
            expect( ")" );
            return e;
        }
        if ( t == "true" || t == "false" )
            return make( expr::boolean, "", nullptr, nullptr, nullptr, t == "true" );
        if ( !t.empty() && t[ 0 ] >= '0' && t[ 0 ] <= '9' )
            return make( expr::num, "", nullptr, nullptr, nullptr, std::stol( t ) );
        if ( is_ident( t ) )
            return make( expr::var, t );
        throw error( "syntax error: unexpected '" + t + "'" );
    }
};

/* ─── types and inference ────────────────────────────────────────── */

/* Types are trees whose leaves are ‹int›, ‹bool› or type variables
 * (numbered); an inner node is a function type with one parameter. */

struct type;
using type_ptr = std::shared_ptr< const type >;

struct type
{
    std::string con;         /* "int", "bool", "->" or "" for a variable */
    int var = -1;
    type_ptr from = nullptr, to = nullptr;
};

type_ptr con( std::string c ) { return std::make_shared< type >( c ); }
type_ptr arrow( type_ptr a, type_ptr b ) { return std::make_shared< type >( "->", -1, a, b ); }

struct scheme { std::set< int > vars; type_ptr body; };

struct inference
{
    std::map< int, type_ptr > subst;
    int counter = 0;

    type_ptr fresh() { return std::make_shared< type >( "", counter++ ); }

    type_ptr apply( const type_ptr &t ) const
    {
        if ( t->con.empty() )
        {
            auto it = subst.find( t->var );
            return it == subst.end() ? t : apply( it->second );
        }
        if ( t->con == "->" )
            return arrow( apply( t->from ), apply( t->to ) );
        return t;
    }

    void free_vars( const type_ptr &t, std::set< int > &out ) const
    {
        type_ptr u = apply( t );
        if ( u->con.empty() ) out.insert( u->var );
        if ( u->con == "->" ) free_vars( u->from, out ), free_vars( u->to, out );
    }

    void unify( const type_ptr &a0, const type_ptr &b0 )
    {
        type_ptr a = apply( a0 ), b = apply( b0 );
        if ( a->con.empty() || b->con.empty() )
        {
            if ( a->con.empty() && b->con.empty() && a->var == b->var )
                return;
            const type_ptr &v = a->con.empty() ? a : b, &t = a->con.empty() ? b : a;
            std::set< int > fv;
            free_vars( t, fv );
            if ( fv.contains( v->var ) )
                throw error( "type error: infinite type" );
            subst.emplace( v->var, t );
            return;
        }
        if ( a->con != b->con )
            throw error( "type error: cannot unify " + show( a ) + " with " + show( b ) );
        if ( a->con == "->" )
            unify( a->from, b->from ), unify( a->to, b->to );
    }

    type_ptr instantiate( const scheme &s )
    {
        std::map< int, type_ptr > m;
        for ( int v : s.vars ) m.emplace( v, fresh() );
        auto copy = [ & ]( auto &self, const type_ptr &t ) -> type_ptr
        {
            if ( t->con.empty() ) return m.contains( t->var ) ? m.at( t->var ) : t;
            if ( t->con == "->" ) return arrow( self( self, t->from ), self( self, t->to ) );
            return t;
        };
        return copy( copy, apply( s.body ) );
    }

    using context = std::map< std::string, scheme >;

    scheme generalize( const context &ctx, const type_ptr &t ) const
    {
        std::set< int > in_type, in_ctx;
        free_vars( t, in_type );
        for ( const auto &[ _, s ] : ctx )
        {
            std::set< int > fv;
            free_vars( s.body, fv );
            for ( int v : s.vars ) fv.erase( v );
            in_ctx.merge( fv );
        }
        scheme out{ {}, apply( t ) };
        for ( int v : in_type )
            if ( !in_ctx.contains( v ) ) out.vars.insert( v );
        return out;
    }

    type_ptr infer( const expr &e, const context &ctx )
    {
        auto int_t = con( "int" ), bool_t = con( "bool" );

        switch ( e.k )
        {
            case expr::num: return int_t;
            case expr::boolean: return bool_t;
            case expr::var:
                if ( auto it = ctx.find( e.name ); it != ctx.end() )
                    return instantiate( it->second );
                throw error( "type error: unbound variable " + e.name );

            case expr::binop:
            {
                type_ptr a = infer( *e.a, ctx ), b = infer( *e.b, ctx );
                if ( e.name == "&&" || e.name == "||" )
                    return unify( a, bool_t ), unify( b, bool_t ), bool_t;
                if ( e.name == "==" || e.name == "!=" )
                {
                    unify( a, b );
                    if ( apply( a )->con == "->" )
                        throw error( "type error: cannot compare functions" );
                    return bool_t;
                }
                unify( a, int_t ), unify( b, int_t );
                return e.name == "+" || e.name == "-" || e.name == "*" || e.name == "/" ? int_t : bool_t;
            }

            case expr::cond:
            {
                unify( infer( *e.a, ctx ), bool_t );
                type_ptr t = infer( *e.b, ctx );
                unify( t, infer( *e.c, ctx ) );
                return t;
            }

            case expr::fn:
            {
                type_ptr p = fresh();
                context inner = ctx;
                inner.insert_or_assign( e.name, scheme{ {}, p } );
                return arrow( p, infer( *e.a, inner ) );
            }

            case expr::app:
            {
                type_ptr f = infer( *e.a, ctx ), x = infer( *e.b, ctx ), r = fresh();
                unify( f, arrow( x, r ) );
                return r;
            }

            case expr::let:
            {
                type_ptr t = infer( *e.a, ctx );
                context inner = ctx;
                inner.insert_or_assign( e.name, generalize( ctx, t ) );
                return infer( *e.b, inner );
            }

            /* ‹let rec›: the name is in scope in its own definition,
             * with a «monomorphic» type; it is generalized only for the
             * body. */
            case expr::letrec:
            {
                type_ptr self = fresh();
                context rec = ctx;
                rec.insert_or_assign( e.name, scheme{ {}, self } );
                unify( self, infer( *e.a, rec ) );
                context inner = ctx;
                inner.insert_or_assign( e.name, generalize( ctx, self ) );
                return infer( *e.b, inner );
            }
        }
        return nullptr;
    }

    /* Printing, with variables renamed to 'a, 'b, … */
    std::string show( const type_ptr &t ) const
    {
        std::map< int, char > names;
        return show( apply( t ), names );
    }

    static std::string show( const type_ptr &t, std::map< int, char > &names )
    {
        if ( t->con.empty() )
        {
            if ( !names.contains( t->var ) )
                names[ t->var ] = 'a' + names.size();
            return std::string( "'" ) + names[ t->var ];
        }
        if ( t->con != "->" )
            return t->con;
        std::string from = show( t->from, names );
        if ( t->from->con == "->" )
            from = "(" + from + ")";
        return from + " -> " + show( t->to, names );
    }
};

/* ─── evaluation ─────────────────────────────────────────────────── */

/* Type checking guarantees that operations are applied to the right
 * kinds of values, so the evaluator does not need to check anything
 * (only division by zero remains). */

struct closure;
using value = std::variant< long, bool, const closure * >;
struct frame { std::map< std::string, value > vars; frame *parent; };
struct closure { const expr *fn; frame *env; };

struct evaluator
{
    std::vector< std::unique_ptr< frame > > frames;
    std::vector< std::unique_ptr< closure > > closures;

    frame *extend( frame *parent, const std::string &n, value v )
    {
        frames.push_back( std::make_unique< frame >( frame{ { { n, v } }, parent } ) );
        return frames.back().get();
    }

    value eval( const expr &e, frame *env )
    {
        switch ( e.k )
        {
            case expr::num: return e.value;
            case expr::boolean: return bool( e.value );
            case expr::var:
                for ( frame *f = env; ; f = f->parent )
                    if ( auto it = f->vars.find( e.name ); it != f->vars.end() )
                        return it->second;
            case expr::binop:
            {
                if ( e.name == "&&" ) return std::get< bool >( eval( *e.a, env ) ) && std::get< bool >( eval( *e.b, env ) );
                if ( e.name == "||" ) return std::get< bool >( eval( *e.a, env ) ) || std::get< bool >( eval( *e.b, env ) );
                value a = eval( *e.a, env ), b = eval( *e.b, env );
                if ( e.name == "==" ) return a == b;
                if ( e.name == "!=" ) return a != b;
                long x = std::get< long >( a ), y = std::get< long >( b );
                if ( e.name == "/" && y == 0 ) throw error( "run-time error: division by zero" );
                const std::string &o = e.name;
                return o == "+" ? value( x + y ) : o == "-" ? value( x - y ) : o == "*" ? value( x * y ) :
                       o == "/" ? value( x / y ) : o == "<" ? value( x < y ) : o == "<=" ? value( x <= y ) :
                       o == ">" ? value( x > y ) : value( x >= y );
            }
            case expr::cond:
                return std::get< bool >( eval( *e.a, env ) ) ? eval( *e.b, env ) : eval( *e.c, env );
            case expr::fn:
                closures.push_back( std::make_unique< closure >( &e, env ) );
                return closures.back().get();
            case expr::app:
            {
                auto c = std::get< const closure * >( eval( *e.a, env ) );
                value arg = eval( *e.b, env );
                return eval( *c->fn->a, extend( c->env, c->fn->name, arg ) );
            }
            case expr::let:
                return eval( *e.b, extend( env, e.name, eval( *e.a, env ) ) );
            case expr::letrec:
            {
                frame *f = extend( env, e.name, 0L );
                f->vars[ e.name ] = eval( *e.a, f );
                return eval( *e.b, f );
            }
        }
        return 0L;
    }
};

/* ─── putting it together ────────────────────────────────────────── */

std::string run( std::string_view src )
{
    try
    {
        parser p{ lex( src ) };
        expr_ptr e = p.expression();
        if ( !p.peek().empty() )
            throw error( "syntax error: unexpected '" + p.peek() + "'" );

        inference w;
        std::string t = w.show( w.infer( *e, {} ) );

        evaluator ev;
        value v = ev.eval( *e, nullptr );
        std::string shown = std::holds_alternative< long >( v ) ? std::to_string( std::get< long >( v ) ) :
                            std::holds_alternative< bool >( v ) ? ( std::get< bool >( v ) ? "true" : "false" ) :
                            "<fn>";
        return shown + " : " + t;
    }
    catch ( const error &e ) { return e.what(); }
}

int main() /* demo */
{
    assert( run( "1 + 2 * 3" ) == "7 : int" );
    assert( run( "fn x => x" ) == "<fn> : 'a -> 'a" );
    assert( run( "fn f g x => f (g x)" ) == "<fn> : ('a -> 'b) -> ('c -> 'a) -> 'c -> 'b" );
    assert( run( "let twice f x = f (f x) in twice (fn n => n * 3) 7" ) == "63 : int" );
    assert( run( "let id x = x in if id true then id 1 else 0" ) == "1 : int" );

    assert( run( "let rec fact n = if n == 0 then 1 else n * fact (n - 1) in fact 10" ) ==
            "3628800 : int" );
    assert( run( "let rec fib n = if n < 2 then n else fib (n - 1) + fib (n - 2) in fib" ) ==
            "<fn> : int -> int" );
    assert( run( "let compose f g x = f (g x) in"
                 " let inc x = x + 1 in"
                 " let double x = x * 2 in"
                 " compose inc double 20 == compose double inc 20" ) == "false : bool" );
    assert( run( "let rec loop f n = if n == 0 then f else loop (fn x => f (f x)) (n - 1) in"
                 " loop (fn x => x + 1) 4 0" ) == "16 : int" );

    assert( run( "1 + true" ) == "type error: cannot unify bool with int" );
    assert( run( "fn x => x x" ) == "type error: infinite type" );
    assert( run( "(fn f => if f true then f 1 else 0) (fn x => x)" ) ==
            "type error: cannot unify bool with int" );
    assert( run( "y" ) == "type error: unbound variable y" );
    assert( run( "let f = fn x => x in" ) == "syntax error: unexpected ''" );
    assert( run( "1 / (2 - 2)" ) == "run-time error: division by zero" );

    return 0;
}
