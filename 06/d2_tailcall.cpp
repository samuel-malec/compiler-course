#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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

/* The interpreter in ‹d1_closures› calls ‹eval› recursively for
 * every function call, so a recursive ‹tiny› function uses C++ stack
 * space proportional to its recursion depth – even a tail-recursive
 * ‘loop’ like this one:
 *
 *     (letrec ((loop (fn (n acc) (if (= n 0) acc (loop (- n 1) (+ acc n))))))
 *       (loop 100000 0))
 *
 * crashes it, as soon as the depth exceeds what fits into the C++
 * stack. In this demo, we restructure ‹eval› so that «tail calls do
 * not grow the C++ stack».
 *
 * The idea: some sub-expressions are evaluated «for» the expression
 * that contains them – their value «is» the value of the whole. These
 * are the branches of ‹if›, the body of ‹let›, and the body of a
 * called function. Instead of evaluating them with a recursive call
 * and returning the result, ‹eval› can simply «replace» the current
 * expression (and environment) with the sub-expression, and go round
 * a loop. Only sub-expressions whose value is needed for further
 * computation – operands, arguments, the condition of ‹if› – are
 * evaluated recursively. Hence a chain of tail calls runs in a
 * single invocation of ‹eval›. */

struct closure;
using value = std::variant< long, const closure * >;
struct frame { std::map< std::string, value > vars; frame *parent; };
struct closure { const sexp *params, *body; frame *env; };
struct error : std::runtime_error { using std::runtime_error::runtime_error; };

struct interpreter
{
    std::vector< std::unique_ptr< frame > > frames;
    std::vector< std::unique_ptr< closure > > closures;
    int depth = 0, max_depth = 0; /* of the C++ recursion, for the demo */

    frame *new_frame( frame *parent )
    {
        frames.push_back( std::make_unique< frame >( frame{ {}, parent } ) );
        return frames.back().get();
    }

    value lookup( frame *env, const std::string &name )
    {
        for ( frame *f = env; f; f = f->parent )
            if ( auto it = f->vars.find( name ); it != f->vars.end() )
                return it->second;
        throw error( "unbound variable " + name );
    }

    static long as_int( value v )
    {
        if ( auto i = std::get_if< long >( &v ) ) return *i;
        throw error( "expected an integer" );
    }

    value eval( const sexp *e, frame *env )
    {
        max_depth = std::max( max_depth, ++ depth );
        value v = eval_loop( e, env );
        -- depth;
        return v;
    }

    value eval_loop( const sexp *e, frame *env )
    {
        while ( true ) /* each iteration handles one expression in tail position */
        {
            if ( e->is_atom() )
            {
                if ( e->atom.find_first_not_of( "-0123456789" ) == e->atom.npos )
                    return std::stol( e->atom );
                return lookup( env, e->atom );
            }

            const auto &it = e->items;
            const std::string &head = it.at( 0 ).atom;

            if ( head == "fn" )
            {
                closures.push_back( std::make_unique< closure >( &it.at( 1 ), &it.at( 2 ), env ) );
                return closures.back().get();
            }

            if ( head == "if" )
            {
                /* the condition is «not» in tail position: recurse … */
                long c = as_int( eval( &it.at( 1 ), env ) );
                /* … but the selected branch is: loop */
                e = c ? &it.at( 2 ) : &it.at( 3 );
                continue;
            }

            if ( head == "letrec" )
            {
                frame *f = new_frame( env );
                for ( const auto &b : it.at( 1 ).items )
                    f->vars[ b.items.at( 0 ).atom ] = 0L;
                for ( const auto &b : it[ 1 ].items )
                    f->vars[ b.items[ 0 ].atom ] = eval( &b.items.at( 1 ), f );
                e = &it.at( 2 ), env = f;
                continue;
            }

            if ( head == "+" || head == "-" || head == "*" || head == "=" )
            {
                long a = as_int( eval( &it.at( 1 ), env ) ), b = as_int( eval( &it.at( 2 ), env ) );
                return head == "+" ? a + b : head == "-" ? a - b : head == "*" ? a * b : a == b;
            }

            /* A call: function and arguments are evaluated recursively,
             * but the body replaces the current expression. */
            value fv = eval( &it[ 0 ], env );
            auto fp = std::get_if< const closure * >( &fv );
            if ( !fp )
                throw error( "calling a non-function" );
            const closure &c = **fp;
            if ( c.params->items.size() != it.size() - 1 )
                throw error( "wrong number of arguments" );

            frame *f = new_frame( c.env );
            for ( std::size_t i = 0; i < c.params->items.size(); ++ i )
                f->vars[ c.params->items[ i ].atom ] = eval( &it[ i + 1 ], env );

            e = c.body, env = f;
        }
    }
};

int main() /* demo */
{
    /* tail recursion: the C++ recursion depth stays small, no matter
     * how many iterations the loop makes */
    sexp loop = read( "(letrec ((loop (fn (n acc) (if (= n 0) acc (loop (- n 1) (+ acc n))))))"
                      "  (loop 50000 0))" );
    interpreter i;
    assert( i.eval( &loop, nullptr ) == value( 1250025000L ) );
    assert( i.max_depth < 10 );

    /* non-tail recursion: the ‹*› needs the value of the recursive
     * call, so each level of the recursion is a level of ‹eval› */
    sexp fact = read( "(letrec ((f (fn (n) (if (= n 0) 1 (* n (f (- n 1))))))) (f 20))" );
    interpreter j;
    assert( j.eval( &fact, nullptr ) == value( 2432902008176640000L ) );
    assert( j.max_depth > 20 );

    /* Note that the «arena» still grows with every call: each call
     * allocates a frame, and frames are only freed at the very end.
     * Tail calls make the «stack» constant, but reclaiming the frames
     * of finished calls needs a garbage collector (Chapter 12). */
    assert( i.frames.size() > 50000 );

    return 0;
}
