#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <map>
#include <memory>

/* The programs in this exercise are written as S-expressions, so
 * that we can concentrate on evaluation instead of parsing. The
 * function ‹read› (provided) turns the text into a tree of ‹sexp›
 * nodes: each node is either an «atom» – a number, a name, a string
 * literal «including» its quotes (no escapes), or anything else
 * without spaces and parentheses – or a «list» of nodes. ‹offset›
 * is the position of the first character of the node in the input.
 * The function ‹write› does the opposite of ‹read›. If the input is
 * malformed, ‹read› throws ‹std::invalid_argument›. */

struct sexp
{
    std::string atom;           /* non-empty for atoms */
    std::vector< sexp > items;  /* elements of a list */
    std::size_t offset = 0;

    bool is_atom() const { return !atom.empty(); }
    bool is_list() const { return atom.empty(); }
};

void skip_space( std::string_view s, std::size_t &pos )
{
    while ( pos < s.size() && ( s[ pos ] == ' ' || s[ pos ] == '\n' ) )
        ++ pos;
}

sexp read_at( std::string_view s, std::size_t &pos )
{
    skip_space( s, pos );
    sexp out{ "", {}, pos };

    if ( pos >= s.size() || s[ pos ] == ')' )
        throw std::invalid_argument( "unexpected end of list or input" );

    if ( s[ pos ] == '(' )
    {
        for ( ++ pos; skip_space( s, pos ), pos >= s.size() || s[ pos ] != ')'; )
            out.items.push_back( read_at( s, pos ) );
        ++ pos;
        return out;
    }

    std::size_t start = pos;

    if ( s[ pos ] == '"' )
    {
        pos = s.find( '"', pos + 1 );
        if ( pos == s.npos )
            throw std::invalid_argument( "unterminated string" );
        ++ pos;
    }
    else
        while ( pos < s.size() && std::string_view( " \n()" ).find( s[ pos ] ) == s.npos )
            ++ pos;

    out.atom = s.substr( start, pos - start );
    return out;
}

sexp read( std::string_view s )
{
    std::size_t pos = 0;
    sexp e = read_at( s, pos );
    skip_space( s, pos );
    if ( pos != s.size() )
        throw std::invalid_argument( "trailing input" );
    return e;
}

std::string write( const sexp &e )
{
    if ( e.is_atom() )
        return e.atom;

    std::string out;
    for ( const auto &i : e.items )
        out += ( out.empty() ? "" : " " ) + write( i );
    return "(" + out + ")";
}

/* In this exercise, we will implement «lazy» (call-by-need)
 * evaluation of ‹let›: in ‹(let x e b)›, the expression ‹e› is «not»
 * evaluated when the ‹let› is entered; instead, ‹x› is bound to a
 * «thunk» – a suspended computation, consisting of the expression
 * ‹e› and the environment in which it should be evaluated. When the
 * value of ‹x› is needed for the first time, the thunk is evaluated
 * and the result is stored in it, so that further uses of ‹x› do not
 * evaluate ‹e› again.
 *
 * The language has integer values only:
 *
 *  • integer literals (digits with an optional leading ‹-›) and
 *    variables (any other atom),
 *  • ‹(+ a b)›, ‹(- a b)›, ‹(* a b)› and ‹(/ a b)›, which evaluate
 *    ‹a› first, then ‹b›; division by zero is an error,
 *  • ‹(if c t e)› – evaluates ‹t› if the value of ‹c› is non-zero,
 *    ‹e› otherwise,
 *  • ‹(let x e b)› – lazy, as described above; note that ‹e› must be
 *    evaluated in the environment of the ‹let›, even if ‹x› is first
 *    needed somewhere where other bindings are visible,
 *  • ‹(trace n e)›, where ‹n› is an integer literal – when evaluated,
 *    it appends ‹n› to the ‹log›, then evaluates to the value of ‹e›;
 *    this is how the tests observe the order of evaluation.
 *
 * Write a function ‹eval( program, log )›, which returns the value
 * of the program, or ‹std::nullopt› if the program is malformed or its
 * evaluation fails (division by zero, unbound variable). Errors in
 * bindings that are never used do not matter. */

// std::optional< long > eval( std::string_view program, std::vector< long > &log );

struct eval_error {};

struct thunk;
using env_t = std::map< std::string, std::shared_ptr< thunk > >;

/* A thunk is either already evaluated (‹value› is set), or it holds
 * the expression and the environment it must be evaluated in. */

struct thunk
{
    const sexp *expr;
    env_t env;
    std::optional< long > value = std::nullopt;
};

bool is_int( const std::string &a )
{
    std::size_t start = a[ 0 ] == '-' ? 1 : 0;
    return a.size() > start && a.find_first_not_of( "0123456789", start ) == a.npos;
}

long eval( const sexp &e, const env_t &env, std::vector< long > &log );

long force( thunk &t, std::vector< long > &log )
{
    if ( !t.value )
    {
        t.value = eval( *t.expr, t.env, log );
        t.env.clear(); /* not needed anymore */
    }
    return *t.value;
}

long eval( const sexp &e, const env_t &env, std::vector< long > &log )
{
    if ( e.is_atom() )
    {
        if ( is_int( e.atom ) )
            return std::stol( e.atom );
        auto it = env.find( e.atom );
        if ( it == env.end() )
            throw eval_error();
        return force( *it->second, log );
    }

    const auto &it = e.items;
    if ( it.empty() || !it[ 0 ].is_atom() )
        throw eval_error();

    const std::string &op = it[ 0 ].atom;
    auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw eval_error(); };

    if ( op == "trace" )
    {
        arity( 2 );
        if ( !it[ 1 ].is_atom() || !is_int( it[ 1 ].atom ) )
            throw eval_error();
        log.push_back( std::stol( it[ 1 ].atom ) );
        return eval( it[ 2 ], env, log );
    }

    if ( op == "let" )
    {
        arity( 3 );
        if ( !it[ 1 ].is_atom() )
            throw eval_error();
        env_t inner = env;
        /* The thunk captures the environment of the ‹let› itself. */
        inner[ it[ 1 ].atom ] = std::make_shared< thunk >( &it[ 2 ], env );
        return eval( it[ 3 ], inner, log );
    }

    if ( op == "if" )
    {
        arity( 3 );
        return eval( it[ 1 ], env, log ) ? eval( it[ 2 ], env, log )
                                         : eval( it[ 3 ], env, log );
    }

    arity( 2 );
    long a = eval( it[ 1 ], env, log );
    long b = eval( it[ 2 ], env, log );

    if ( op == "+" ) return a + b;
    if ( op == "-" ) return a - b;
    if ( op == "*" ) return a * b;
    if ( op == "/" && b != 0 ) return a / b;
    throw eval_error();
}

std::optional< long > eval( std::string_view program, std::vector< long > &log )
{
    try
    {
        sexp e = read( program );
        return eval( e, env_t{}, log );
    }
    catch ( const eval_error & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    using v = std::vector< long >;
    v log;

    auto ev = [ & ]( std::string_view p ) { log.clear(); return eval( p, log ); };

    assert( ev( "(+ (trace 1 1) (trace 2 2))" ) == 3 );
    assert(( log == v{ 1, 2 } ));

    assert( ev( "(let x (trace 1 5) (+ x x))" ) == 10 );
    assert( log == v{ 1 } );

    assert( ev( "(let x (trace 1 5) 7)" ) == 7 );
    assert( log == v{} );

    assert( ev( "(let x (trace 1 2) (let y (trace 2 3) (+ y x)))" ) == 5 );
    assert(( log == v{ 2, 1 } ));

    assert( ev( "(let x (trace 1 1) (let y (trace 2 (+ x x)) (+ y y)))" ) == 4 );
    assert(( log == v{ 2, 1 } ));

    assert( ev( "(if (trace 1 0) (trace 2 5) (trace 3 6))" ) == 6 );
    assert(( log == v{ 1, 3 } ));

    assert( ev( "(let x (trace 1 (/ 1 0)) 0)" ) == 0 );
    assert( log == v{} );

    assert( !ev( "(let x (trace 1 (/ 1 0)) x)" ) );
    assert( log == v{ 1 } );

    assert( ev( "(let x 1 (let y x (let x 2 y)))" ) == 1 );
    assert( ev( "(let x (trace 1 1) (let y (trace 2 x) (let x 5 (+ y x))))" ) == 6 );
    assert(( log == v{ 2, 1 } ));

    assert( ev( "(let x u 3)" ) == 3 );
    assert( !ev( "(let x u x)" ) );
    assert( !ev( "(trace x 1)" ) );
    assert( !ev( "(let x 1)" ) );

    return 0;
}
