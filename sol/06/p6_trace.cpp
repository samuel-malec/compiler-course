#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <expected>
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

/* A good error message for a run-time error says not only «what»
 * went wrong, but also «how the program got there» – the chain of
 * function calls which were in progress at the time, the «stack
 * trace». Write an evaluator for the language of ‹p1_letrec› which
 * reports run-time errors with a stack trace.
 *
 * Functions are anonymous, so we need a way to name them: a closure
 * created by a ‹fn› expression which is «directly» the value of a
 * ‹let› or ‹letrec› binding gets the name of the bound variable; any
 * other closure is called ‹lambda›. On error, the result is a list of
 * strings: first the message, then ‹in NAME› for each call that was in
 * progress, the innermost first. The messages are:
 *
 *  • ‹unbound variable X›,
 *  • ‹division by zero›,
 *  • ‹not an integer› (an operand of an operator or the condition of
 *    ‹if› is a function),
 *  • ‹not a function› (the callee of a call is an integer),
 *  • ‹wrong number of arguments› – this error happens in the «caller»
 *    (the callee is not entered).
 *
 * A malformed program gives the single message ‹syntax error›.
 * Otherwise, the result is the integer value of the program (or the
 * message ‹not an integer› if it is a function). */

// std::expected< long, std::vector< std::string > > eval( std::string_view program );

struct closure;
using value = std::variant< long, const closure * >;
struct frame { std::map< std::string, value > vars; frame *parent; };
struct closure { const sexp *params, *body; frame *env; std::string name; };

struct runtime_error { std::string message; };
struct syntax_error {};

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

bool is_fn( const sexp &e ) { return e.is_list() && !e.items.empty() && e.items[ 0 ].atom == "fn"; }

struct interpreter
{
    std::vector< std::unique_ptr< frame > > frames = {};
    std::vector< std::unique_ptr< closure > > closures = {};
    std::vector< std::string > stack = {}; /* names of active calls, outermost first */

    frame *new_frame( frame *parent )
    {
        frames.push_back( std::make_unique< frame >( frame{ {}, parent } ) );
        return frames.back().get();
    }

    static long as_int( value v )
    {
        if ( auto i = std::get_if< long >( &v ) ) return *i;
        throw runtime_error{ "not an integer" };
    }

    /* Evaluate ‹e›; if it is a ‹fn›, name the closure ‹name›. */
    value named( const sexp &e, frame *env, const std::string &name )
    {
        value v = eval( e, env );
        if ( is_fn( e ) )
            closures.back()->name = name;
        return v;
    }

    value eval( const sexp &e, frame *env )
    {
        if ( e.is_atom() )
        {
            if ( is_number( e.atom ) ) return std::stol( e.atom );
            for ( frame *f = env; f; f = f->parent )
                if ( auto it = f->vars.find( e.atom ); it != f->vars.end() )
                    return it->second;
            throw runtime_error{ "unbound variable " + e.atom };
        }

        const auto &it = e.items;
        if ( it.empty() ) throw syntax_error();
        const std::string &head = it[ 0 ].atom;

        if ( head == "fn" )
        {
            if ( it.size() != 3 || !it[ 1 ].is_list() ) throw syntax_error();
            closures.push_back( std::make_unique< closure >( &it[ 1 ], &it[ 2 ], env, "lambda" ) );
            return closures.back().get();
        }
        if ( head == "if" )
        {
            if ( it.size() != 4 ) throw syntax_error();
            return as_int( eval( it[ 1 ], env ) ) ? eval( it[ 2 ], env ) : eval( it[ 3 ], env );
        }
        if ( head == "let" )
        {
            if ( it.size() != 4 || !it[ 1 ].is_atom() ) throw syntax_error();
            value v = named( it[ 2 ], env, it[ 1 ].atom );
            frame *f = new_frame( env );
            f->vars[ it[ 1 ].atom ] = v;
            return eval( it[ 3 ], f );
        }
        if ( head == "letrec" )
        {
            if ( it.size() != 3 || !it[ 1 ].is_list() ) throw syntax_error();
            frame *f = new_frame( env );
            for ( const auto &b : it[ 1 ].items )
                if ( !b.is_list() || b.items.size() != 2 || !b.items[ 0 ].is_atom() ||
                     !is_fn( b.items[ 1 ] ) || !f->vars.emplace( b.items[ 0 ].atom, 0L ).second )
                    throw syntax_error();
            for ( const auto &b : it[ 1 ].items )
                f->vars[ b.items[ 0 ].atom ] = named( b.items[ 1 ], f, b.items[ 0 ].atom );
            return eval( it[ 2 ], f );
        }
        if ( is_op( head ) )
        {
            if ( it.size() != 3 ) throw syntax_error();
            long a = as_int( eval( it[ 1 ], env ) ), b = as_int( eval( it[ 2 ], env ) );
            if ( ( head == "/" || head == "%" ) && b == 0 )
                throw runtime_error{ "division by zero" };
            return head == "+" ? a + b : head == "-" ? a - b : head == "*" ? a * b :
                   head == "/" ? a / b : head == "%" ? a % b : head == "<" ? a < b :
                   head == "<=" ? a <= b : head == ">" ? a > b : head == ">=" ? a >= b :
                   head == "=" ? a == b : a != b;
        }

        value fv = eval( it[ 0 ], env );
        auto fp = std::get_if< const closure * >( &fv );
        if ( !fp )
            throw runtime_error{ "not a function" };
        const closure &c = **fp;
        if ( c.params->items.size() != it.size() - 1 )
            throw runtime_error{ "wrong number of arguments" };

        frame *f = new_frame( c.env );
        for ( std::size_t i = 0; i < c.params->items.size(); ++ i )
            f->vars[ c.params->items[ i ].atom ] = eval( it[ i + 1 ], env );

        /* The stack is only popped on a normal return: when an error
         * is thrown, it still holds all the calls in progress. */
        stack.push_back( c.name );
        value v = eval( *c.body, f );
        stack.pop_back();
        return v;
    }
};

std::expected< long, std::vector< std::string > > eval( std::string_view program )
{
    interpreter i;
    try
    {
        sexp p = read( program );
        return interpreter::as_int( i.eval( p, nullptr ) );
    }
    catch ( const runtime_error &e )
    {
        std::vector< std::string > out{ e.message };
        for ( auto it = i.stack.rbegin(); it != i.stack.rend(); ++ it )
            out.push_back( "in " + *it );
        return std::unexpected( out );
    }
    catch ( const syntax_error & ) { return std::unexpected( std::vector< std::string >{ "syntax error" } ); }
    catch ( const std::invalid_argument & ) { return std::unexpected( std::vector< std::string >{ "syntax error" } ); }
}

int main()
{
    using v = std::vector< std::string >;
    auto err = []( std::string_view p ) { return eval( p ).error(); };

    assert( eval( "(letrec ((f (fn (n) (* n 2)))) (f 21))" ) == 42 );

    assert(( err( "(letrec ((f (fn (n) (/ 1 n))) (g (fn (n) (f (- n 1))))) (g 1))" ) ==
             v{ "division by zero", "in f", "in g" } ));
    assert(( err( "(+ 1 x)" ) == v{ "unbound variable x" } ));
    assert(( err( "(let h (fn (x) (x 1)) (h 5))" ) == v{ "not a function", "in h" } ));
    assert(( err( "(let h (fn (x) (+ x 1)) (let k (fn () (h h)) (k)))" ) ==
             v{ "not an integer", "in h", "in k" } ));
    assert(( err( "(let h (fn (x) x) (let k (fn () (h)) (k)))" ) ==
             v{ "wrong number of arguments", "in k" } ));
    assert(( err( "(let apply (fn (f) (f 0)) (apply (fn (x) (/ 1 x))))" ) ==
             v{ "division by zero", "in lambda", "in apply" } ));
    assert(( err( "(letrec ((f (fn (n) (if (= n 0) z (f (- n 1)))))) (f 2))" ) ==
             v{ "unbound variable z", "in f", "in f", "in f" } ));
    assert(( err( "(let f (let g (fn () 1) (fn () (g 1))) (f))" ) ==
             v{ "wrong number of arguments", "in lambda" } ));
    assert(( err( "(fn () 1)" ) == v{ "not an integer" } ));
    assert(( err( "(let x)" ) == v{ "syntax error" } ));

    return 0;
}
