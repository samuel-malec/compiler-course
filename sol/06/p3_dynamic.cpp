#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

/* Write an evaluator for the language of ‹e1_apply› which supports
 * both «lexical» and «dynamic» scoping, selected by the ‹dynamic›
 * argument. With lexical scoping, the function body is evaluated in
 * the environment where the function was «created» (extended with the
 * parameters), as usual. With dynamic scoping, it is evaluated in the
 * environment of the «call» (again extended with the parameters); a
 * function value is then just its code, there is nothing to capture.
 *
 * Return the integer value of the program, or ‹std::nullopt› on any
 * error, as in ‹e1_apply›. Look carefully at the tests – they show
 * the most important differences between the two disciplines. */

// std::optional< long > eval( std::string_view program, bool dynamic );

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
    bool dynamic = false;

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

        /* the only difference: whose environment is the parent */
        frame *f = new_frame( dynamic ? env : c.env );
        for ( std::size_t i = 0; i < c.params->items.size(); ++ i )
            f->vars[ c.params->items[ i ].atom ] = eval( it[ i + 1 ], env );
        return eval( *c.body, f );
    }
};

std::optional< long > eval( std::string_view program, bool dynamic )
{
    try
    {
        sexp p = read( program );
        interpreter i;
        i.dynamic = dynamic;
        value v = i.eval( p, nullptr );
        if ( auto n = std::get_if< long >( &v ) ) return *n;
        return std::nullopt;
    }
    catch ( const bad & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    auto both = []( std::string_view p ) { return std::pair{ eval( p, false ), eval( p, true ) }; };
    using r = std::pair< std::optional< long >, std::optional< long > >;

    /* no difference */
    assert(( both( "((fn (x y) (- x y)) 5 3)" ) == r{ 2, 2 } ));

    /* which ‹x› does ‹f› see? */
    assert(( both( "(let x 1 (let f (fn () x) (let x 2 (f))))" ) == r{ 1, 2 } ));

    /* a ‘free’ variable can be supplied by the caller */
    assert(( both( "(let f (fn () y) (let y 5 (f)))" ) == r{ std::nullopt, 5 } ));

    /* recursion without ‹letrec›: the caller's environment contains ‹f› */
    assert(( both( "(let f (fn (n) (if n (* n (f (- n 1))) 1)) (f 5))" ) == r{ std::nullopt, 120 } ));

    /* closures do not work: ‹x› is gone by the time the inner function runs */
    assert(( both( "(let add (fn (x) (fn (y) (+ x y))) ((add 1) 2))" ) == r{ 3, std::nullopt } ));

    /* … unless the caller happens to have an ‹x› of its own */
    assert(( both( "(let add (fn (x) (fn (y) (+ x y))) (let x 100 ((add 1) 2)))" ) == r{ 3, 102 } ));

    /* parameters shadow the caller's variables in both cases */
    assert(( both( "(let x 1 (let f (fn (x) x) (f 7)))" ) == r{ 7, 7 } ));

    return 0;
}
