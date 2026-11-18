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

/* So far, all our languages have been purely functional: a variable,
 * once bound, never changes. In this exercise, we add «mutable»
 * variables and loops – a first taste of the imperative features of
 * the next chapter. All values are integers. The forms are:
 *
 *  • integer literals (digits with an optional leading ‹-›) and
 *    variables (any other atom),
 *  • ‹(+ a b)›, ‹(- a b)›, ‹(* a b)›, ‹(/ a b)›, ‹(% a b)› (‹/› and
 *    ‹%› as in C++, by zero is an error), ‹(< a b)› and ‹(= a b)›
 *    (1 for true, 0 for false),
 *  • ‹(if c t e)› – ‹c› is true if non-zero,
 *  • ‹(let x v b)› – creates a «new» variable ‹x›, initialized to the
 *    value of ‹v›, which is visible in ‹b› (and only there),
 *  • ‹(set x v)› – assigns the value of ‹v› to the «nearest» existing
 *    variable called ‹x› (it is an error if there is none); the value
 *    of the whole expression is the assigned value,
 *  • ‹(do e₁ e₂ … eₙ)› – evaluates the expressions in order; the
 *    value is that of ‹eₙ› (‹n ≥ 1›),
 *  • ‹(while c b)› – evaluates ‹b› repeatedly as long as ‹c› is
 *    true; its value is 0.
 *
 * Notice that ‹set› inside the body of a ‹let› can change a variable
 * bound «outside» it, and that change must be visible after the
 * ‹let› ends. The environment chain from ‹e2_env› is a good fit.
 *
 * Write a function ‹run› which returns the value of the program, or
 * ‹std::nullopt› if it is malformed or fails. */

// std::optional< long > run( std::string_view program );

struct run_error {};

struct frame
{
    std::map< std::string, long > vars;
    std::shared_ptr< frame > parent;

    long *find( const std::string &name )
    {
        for ( frame *f = this; f; f = f->parent.get() )
            if ( auto it = f->vars.find( name ); it != f->vars.end() )
                return &it->second;
        return nullptr;
    }
};

using env_t = std::shared_ptr< frame >;

bool is_int( const std::string &a )
{
    std::size_t start = a[ 0 ] == '-' ? 1 : 0;
    return a.size() > start && a.find_first_not_of( "0123456789", start ) == a.npos;
}

long eval( const sexp &e, const env_t &env )
{
    if ( e.is_atom() )
    {
        if ( is_int( e.atom ) )
            return std::stol( e.atom );
        if ( long *v = env->find( e.atom ) )
            return *v;
        throw run_error();
    }

    const auto &it = e.items;
    if ( it.empty() || !it[ 0 ].is_atom() )
        throw run_error();

    const std::string &op = it[ 0 ].atom;
    auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw run_error(); };

    if ( op == "do" )
    {
        if ( it.size() < 2 )
            throw run_error();
        long v = 0;
        for ( std::size_t i = 1; i < it.size(); ++ i )
            v = eval( it[ i ], env );
        return v;
    }

    if ( op == "while" )
    {
        arity( 2 );
        while ( eval( it[ 1 ], env ) )
            eval( it[ 2 ], env );
        return 0;
    }

    if ( op == "if" )
    {
        arity( 3 );
        return eval( it[ 1 ], env ) ? eval( it[ 2 ], env ) : eval( it[ 3 ], env );
    }

    if ( op == "let" )
    {
        arity( 3 );
        if ( !it[ 1 ].is_atom() )
            throw run_error();
        auto inner = std::make_shared< frame >();
        inner->parent = env;
        inner->vars[ it[ 1 ].atom ] = eval( it[ 2 ], env );
        return eval( it[ 3 ], inner );
    }

    if ( op == "set" )
    {
        arity( 2 );
        long v = eval( it[ 2 ], env );
        long *target = it[ 1 ].is_atom() ? env->find( it[ 1 ].atom ) : nullptr;
        if ( !target )
            throw run_error();
        return *target = v;
    }

    arity( 2 );
    long a = eval( it[ 1 ], env ), b = eval( it[ 2 ], env );

    if ( op == "+" ) return a + b;
    if ( op == "-" ) return a - b;
    if ( op == "*" ) return a * b;
    if ( op == "<" ) return a < b;
    if ( op == "=" ) return a == b;
    if ( ( op == "/" || op == "%" ) && b != 0 )
        return op == "/" ? a / b : a % b;
    throw run_error();
}

std::optional< long > run( std::string_view program )
{
    try
    {
        return eval( read( program ), std::make_shared< frame >() );
    }
    catch ( const run_error & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    assert( run( "(let x 1 (do (set x (+ x 1)) (* x 10)))" ) == 20 );
    assert( run( "(let x 1 (do (let y 2 (set x y)) x))" ) == 2 );
    assert( run( "(let x 1 (do (let x 5 (set x 7)) x))" ) == 1 );

    auto fact = "(let n 10 (let acc 1 (do"
                "  (while (< 0 n) (do (set acc (* acc n)) (set n (- n 1))))"
                "  acc)))";
    assert( run( fact ) == 3628800 );

    auto gcd = "(let a 1071 (let b 462 (do"
               "  (while (< 0 b) (let t b (do (set b (% a b)) (set a t))))"
               "  a)))";
    assert( run( gcd ) == 21 );

    auto collatz = "(let n 27 (let steps 0 (do"
                   "  (while (< 1 n) (do"
                   "    (if (= (% n 2) 0) (set n (/ n 2)) (set n (+ (* 3 n) 1)))"
                   "    (set steps (+ steps 1))))"
                   "  steps)))";
    assert( run( collatz ) == 111 );

    auto fib = "(let a 0 (let b 1 (let i 0 (do"
               "  (while (< i 50) (let t (+ a b) (do (set a b) (set b t) (set i (+ i 1)))))"
               "  a))))";
    assert( run( fib ) == 12586269025 );

    assert( run( "(while 0 (/ 1 0))" ) == 0 );
    assert( run( "(do 1 2 3)" ) == 3 );

    assert( !run( "(set x 1)" ) );
    assert( !run( "(do)" ) );
    assert( !run( "(let x 0 (while (< x 3) (set y x)))" ) );
    assert( !run( "(% 1 0)" ) );
    assert( !run( "(let x 1 (set x))" ) );

    return 0;
}
