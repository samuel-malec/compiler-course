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

/* Write an evaluator for the language of ‹p1_letrec› (with
 * ‹letrec›), which implements «proper tail calls»: evaluating an
 * expression in tail position – a branch of ‹if›, the body of ‹let›
 * or ‹letrec›, or the body of a called function – must not use any
 * additional C++ stack space (see ‹d2_tailcall›). Consequently,
 * tail-recursive functions can run for any number of iterations.
 * Non-tail recursion (like ‹(* n (fact (- n 1)))›) may still use the
 * C++ stack, and the tests only use it to a modest depth.
 *
 * Return the integer value of the program, or ‹std::nullopt› on the
 * errors of ‹p1_letrec›. */

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

    /* Expressions in tail position replace ‹e› and ‹env› and go round
     * the loop, instead of recursing (see ‹d2_tailcall›). */

    value eval( const sexp *e, frame *env )
    {
        while ( true )
        {
            if ( e->is_atom() )
            {
                if ( is_number( e->atom ) ) return std::stol( e->atom );
                for ( frame *f = env; f; f = f->parent )
                    if ( auto it = f->vars.find( e->atom ); it != f->vars.end() )
                        return it->second;
                throw bad();
            }

            const auto &it = e->items;
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
                e = as_int( eval( &it[ 1 ], env ) ) ? &it[ 2 ] : &it[ 3 ];
                continue;
            }
            if ( head == "let" )
            {
                if ( it.size() != 4 || !it[ 1 ].is_atom() ) throw bad();
                value v = eval( &it[ 2 ], env );
                frame *f = new_frame( env );
                f->vars[ it[ 1 ].atom ] = v;
                e = &it[ 3 ], env = f;
                continue;
            }
            if ( head == "letrec" )
            {
                if ( it.size() != 3 || !it[ 1 ].is_list() ) throw bad();
                frame *f = new_frame( env );
                for ( const auto &b : it[ 1 ].items )
                    if ( !b.is_list() || b.items.size() != 2 || !b.items[ 0 ].is_atom() ||
                         !b.items[ 1 ].is_list() || b.items[ 1 ].items.empty() ||
                         b.items[ 1 ].items[ 0 ].atom != "fn" ||
                         !f->vars.emplace( b.items[ 0 ].atom, 0L ).second )
                        throw bad();
                for ( const auto &b : it[ 1 ].items )
                    f->vars[ b.items[ 0 ].atom ] = eval( &b.items[ 1 ], f );
                e = &it[ 2 ], env = f;
                continue;
            }
            if ( is_op( head ) )
            {
                if ( it.size() != 3 ) throw bad();
                long a = as_int( eval( &it[ 1 ], env ) ), b = as_int( eval( &it[ 2 ], env ) );
                return arith( head, a, b );
            }

            value fv = eval( &it[ 0 ], env );
            auto fp = std::get_if< const closure * >( &fv );
            if ( !fp || ( *fp )->params->items.size() != it.size() - 1 ) throw bad();
            const closure &c = **fp;

            frame *f = new_frame( c.env );
            for ( std::size_t i = 0; i < c.params->items.size(); ++ i )
                f->vars[ c.params->items[ i ].atom ] = eval( &it[ i + 1 ], env );
            e = c.body, env = f;
        }
    }
};

std::optional< long > eval( std::string_view program )
{
    try
    {
        sexp p = read( program );
        interpreter i;
        value v = i.eval( &p, nullptr );
        if ( auto n = std::get_if< long >( &v ) ) return *n;
        return std::nullopt;
    }
    catch ( const bad & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    const char *loop = "(letrec ((loop (fn (n acc) (if (= n 0) acc (loop (- n 1) (+ acc 1))))))"
                       " (loop 60000 0))";
    assert( eval( loop ) == 60000 );

    const char *mutual = "(letrec ((even (fn (n) (if (= n 0) 1 (odd (- n 1)))))"
                         "         (odd (fn (n) (if (= n 0) 0 (even (- n 1))))))"
                         " (even 60001))";
    assert( eval( mutual ) == 0 );

    /* a tail call through a ‹let› body and a higher-order function */
    const char *ho = "(letrec ((apply (fn (f x) (f x)))"
                     "         (count (fn (n) (if (= n 0) 7 (let m (- n 1) (apply count m))))))"
                     " (count 30000))";
    assert( eval( ho ) == 7 );

    assert( eval( "(letrec ((f (fn (n) (if (= n 0) 0 (+ 1 (f (- n 1))))))) (f 1000))" ) == 1000 );
    assert( !eval( "(letrec ((f (fn (n) (if (= n 0) (/ 1 0) (f (- n 1)))))) (f 100))" ) );

    return 0;
}
