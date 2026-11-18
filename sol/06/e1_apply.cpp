#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <variant>

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

/* Write an evaluator for the language from the introduction (without
 * ‹letrec›). Functions can have any number of parameters (including
 * zero), and a call must supply exactly as many arguments. The
 * arguments are evaluated left to right, after the function. The
 * function ‹eval› returns the value of the program, which must be an
 * integer. Return ‹std::nullopt› if the program is malformed, if its
 * value is a function, or if evaluation fails: an unbound variable, a
 * call of a non-function, a wrong number of arguments, an arithmetic
 * operation or ‹if› condition on a function, or division by zero. */

// std::optional< long > eval( std::string_view program );

struct bad {};

struct closure;
using value = std::variant< long, const closure * >;
struct frame { std::map< std::string, value > vars; frame *parent; };
struct closure { const sexp *params, *body; frame *env; };

bool is_number( const std::string &a )
{
    std::size_t s = !a.empty() && a[ 0 ] == '-' ? 1 : 0;
    return a.size() > s && a.find_first_not_of( "0123456789", s ) == a.npos;
}

bool is_op( const std::string &h )
{
    static const std::string_view ops[] = { "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "=", "!=" };
    for ( auto o : ops ) if ( h == o ) return true;
    return false;
}

long arith( const std::string &op, long a, long b )
{
    if ( ( op == "/" || op == "%" ) && b == 0 ) throw bad();
    return op == "+" ? a + b : op == "-" ? a - b : op == "*" ? a * b :
           op == "/" ? a / b : op == "%" ? a % b : op == "<" ? a < b :
           op == "<=" ? a <= b : op == ">" ? a > b : op == ">=" ? a >= b :
           op == "=" ? a == b : a != b;
}

struct interpreter
{
    std::vector< std::unique_ptr< frame > > frames = {};
    std::vector< std::unique_ptr< closure > > closures = {};

    frame *new_frame( frame *parent )
    {
        frames.push_back( std::make_unique< frame >( frame{ {}, parent } ) );
        return frames.back().get();
    }

    static long as_int( value v )
    {
        if ( auto i = std::get_if< long >( &v ) ) return *i;
        throw bad();
    }

    value eval( const sexp &e, frame *env )
    {
        if ( e.is_atom() )
        {
            if ( is_number( e.atom ) ) return std::stol( e.atom );
            for ( frame *f = env; f; f = f->parent )
                if ( auto it = f->vars.find( e.atom ); it != f->vars.end() )
                    return it->second;
            throw bad();
        }

        const auto &it = e.items;
        if ( it.empty() ) throw bad();
        const std::string &head = it[ 0 ].atom;

        if ( head == "fn" )
        {
            if ( it.size() != 3 || !it[ 1 ].is_list() ) throw bad();
            closures.push_back( std::make_unique< closure >( &it[ 1 ], &it[ 2 ], env ) );
            return closures.back().get();
        }
        if ( head == "if" )
        {
            if ( it.size() != 4 ) throw bad();
            return as_int( eval( it[ 1 ], env ) ) ? eval( it[ 2 ], env ) : eval( it[ 3 ], env );
        }
        if ( head == "let" )
        {
            if ( it.size() != 4 || !it[ 1 ].is_atom() ) throw bad();
            value v = eval( it[ 2 ], env );
            frame *f = new_frame( env );
            f->vars[ it[ 1 ].atom ] = v;
            return eval( it[ 3 ], f );
        }
        if ( is_op( head ) )
        {
            if ( it.size() != 3 ) throw bad();
            long a = as_int( eval( it[ 1 ], env ) ), b = as_int( eval( it[ 2 ], env ) );
            return arith( head, a, b );
        }

        value fv = eval( it[ 0 ], env );
        auto fp = std::get_if< const closure * >( &fv );
        if ( !fp || ( *fp )->params->items.size() != it.size() - 1 ) throw bad();
        const closure &c = **fp;

        frame *f = new_frame( c.env );
        for ( std::size_t i = 0; i < c.params->items.size(); ++ i )
            f->vars[ c.params->items[ i ].atom ] = eval( it[ i + 1 ], env );
        return eval( *c.body, f );
    }
};

std::optional< long > eval( std::string_view program )
{
    try
    {
        sexp p = read( program );
        interpreter i;
        value v = i.eval( p, nullptr );
        if ( auto n = std::get_if< long >( &v ) ) return *n;
        return std::nullopt;
    }
    catch ( const bad & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    assert( eval( "42" ) == 42 );
    assert( eval( "((fn (x) (* x x)) 7)" ) == 49 );
    assert( eval( "((fn () 5))" ) == 5 );
    assert( eval( "((fn (a b c) (- a (- b c))) 10 5 1)" ) == 6 );
    assert( eval( "(let add (fn (x) (fn (y) (+ x y))) ((add 2) 3))" ) == 5 );
    assert( eval( "(let x 1 (let f (fn (y) (+ x y)) (let x 100 (f 1))))" ) == 2 );
    assert( eval( "(let twice (fn (f) (fn (x) (f (f x)))) ((twice (twice (fn (n) (* 2 n)))) 1))" ) == 16 );
    assert( eval( "(let k (fn (a) (fn (b) a)) ((k 1) 2))" ) == 1 );
    assert( eval( "(if (< 1 2) ((fn (x) x) 3) (1 2))" ) == 3 );

    assert( !eval( "(fn (x) x)" ) );
    assert( !eval( "(f 1)" ) );
    assert( !eval( "(1 2)" ) );
    assert( !eval( "((fn (x) x))" ) );
    assert( !eval( "((fn (x) x) 1 2)" ) );
    assert( !eval( "(+ 1 (fn (x) x))" ) );
    assert( !eval( "(if (fn (x) x) 1 2)" ) );
    assert( !eval( "(let f (fn (n) (if n (f (- n 1)) 0)) (f 3))" ) );
    assert( !eval( "(/ 1 0)" ) );

    return 0;
}
