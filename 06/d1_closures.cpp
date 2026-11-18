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

/* In this demo, we write an interpreter for the language described
 * in the introduction, extended with ‹letrec›:
 *
 *     (letrec ((f₁ e₁) (f₂ e₂) …) body)
 *
 * which binds ‹f₁›, ‹f₂›, … in an environment where all of them are
 * already visible – so that ‹eᵢ› (usually functions) can refer to
 * themselves and to each other.
 *
 * Values are integers and closures. A closure holds pointers to its
 * parameter list and body (inside the syntax tree, which outlives
 * the evaluation), and to the frame in which it was created. */

struct closure;
using value = std::variant< long, const closure * >;

struct frame
{
    std::map< std::string, value > vars;
    frame *parent;
};

struct closure
{
    const sexp *params, *body;
    frame *env;
};

struct error : std::runtime_error { using std::runtime_error::runtime_error; };

/* All frames and closures live in an «arena» owned by the
 * interpreter (see the footnote in the introduction): they can point
 * to each other freely, even in cycles, and everything is freed when
 * the interpreter goes away. */

struct interpreter
{
    std::vector< std::unique_ptr< frame > > frames;
    std::vector< std::unique_ptr< closure > > closures;

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

    static bool is_number( const sexp &e )
    {
        return e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos && e.atom != "-";
    }

    value eval( const sexp &e, frame *env )
    {
        if ( e.is_atom() )
            return is_number( e ) ? value( std::stol( e.atom ) ) : lookup( env, e.atom );

        const std::string &head = e.items.at( 0 ).atom;

        /* A function expression evaluates to a closure which
         * «captures» the current environment. Nothing else happens:
         * the body is not evaluated until the closure is called. */
        if ( head == "fn" )
        {
            closures.push_back( std::make_unique< closure >( &e.items.at( 1 ), &e.items.at( 2 ), env ) );
            return closures.back().get();
        }

        if ( head == "if" )
            return as_int( eval( e.items.at( 1 ), env ) ) ? eval( e.items.at( 2 ), env )
                                                          : eval( e.items.at( 3 ), env );

        if ( head == "let" )
        {
            value v = eval( e.items.at( 2 ), env );
            frame *f = new_frame( env );
            f->vars[ e.items[ 1 ].atom ] = v;
            return eval( e.items.at( 3 ), f );
        }

        /* ‹letrec›: create the frame «first», then evaluate the
         * definitions «in that frame», and only then fill in the
         * values. A closure created by ‹eᵢ› captures the new frame, and
         * by the time it is called, the frame contains all the
         * definitions – including the closure itself. (Evaluating ‹eᵢ›
         * must not «use» any of the names, though: they have no value
         * yet. This is why ‹letrec› is normally restricted to
         * functions.) */
        if ( head == "letrec" )
        {
            frame *f = new_frame( env );
            for ( const auto &b : e.items.at( 1 ).items )
                f->vars[ b.items.at( 0 ).atom ] = 0L; /* placeholder */
            for ( const auto &b : e.items[ 1 ].items )
                f->vars[ b.items[ 0 ].atom ] = eval( b.items.at( 1 ), f );
            return eval( e.items.at( 2 ), f );
        }

        static const std::string ops[] = { "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "=", "!=" };
        for ( const auto &op : ops )
            if ( head == op )
            {
                long a = as_int( eval( e.items.at( 1 ), env ) );
                long b = as_int( eval( e.items.at( 2 ), env ) );
                if ( ( op == "/" || op == "%" ) && b == 0 )
                    throw error( "division by zero" );
                return op == "+" ? a + b : op == "-" ? a - b : op == "*" ? a * b :
                       op == "/" ? a / b : op == "%" ? a % b : op == "<" ? a < b :
                       op == "<=" ? a <= b : op == ">" ? a > b : op == ">=" ? a >= b :
                       op == "=" ? a == b : a != b;
            }

        /* A call: evaluate the function and the arguments (left to
         * right), then evaluate the body in a «new frame» whose parent
         * is the frame «captured by the closure» – not ‹env›! This one
         * line is the difference between lexical and dynamic scoping. */
        value fv = eval( e.items[ 0 ], env );
        auto fp = std::get_if< const closure * >( &fv );
        if ( !fp )
            throw error( "calling a non-function" );
        const closure &c = **fp;

        if ( c.params->items.size() != e.items.size() - 1 )
            throw error( "wrong number of arguments" );

        frame *f = new_frame( c.env );
        for ( std::size_t i = 0; i < c.params->items.size(); ++ i )
            f->vars[ c.params->items[ i ].atom ] = eval( e.items[ i + 1 ], env );
        return eval( *c.body, f );
    }
};

std::string run( std::string_view src )
{
    sexp program = read( src );
    interpreter i;
    try
    {
        value v = i.eval( program, nullptr );
        if ( auto n = std::get_if< long >( &v ) )
            return std::to_string( *n );
        return "<closure>";
    }
    catch ( const error &e ) { return e.what(); }
}

int main() /* demo */
{
    /* closures capture their environment */
    assert( run( "(let add (fn (x) (fn (y) (+ x y))) (let inc (add 1) (inc 41)))" ) == "42" );

    /* … the environment of their «definition», not of the call */
    assert( run( "(let x 1 (let f (fn () x) (let x 2 (f))))" ) == "1" );

    /* higher-order functions */
    assert( run( "(let twice (fn (f x) (f (f x))) (twice (fn (n) (* n 3)) 7))" ) == "63" );
    assert( run( "(let compose (fn (f g) (fn (x) (f (g x))))"
                 " ((compose (fn (x) (+ x 1)) (fn (x) (* x 2))) 5))" ) == "11" );

    /* recursion needs ‹letrec› … */
    assert( run( "(letrec ((fact (fn (n) (if (= n 0) 1 (* n (fact (- n 1)))))))"
                 " (fact 10))" ) == "3628800" );
    assert( run( "(let fact (fn (n) (if (= n 0) 1 (* n (fact (- n 1))))) (fact 3))" ) ==
            "unbound variable fact" );

    /* … which also supports mutual recursion */
    assert( run( "(letrec ((even (fn (n) (if (= n 0) 1 (odd (- n 1)))))"
                 "         (odd (fn (n) (if (= n 0) 0 (even (- n 1))))))"
                 " (even 101))" ) == "0" );

    assert( run( "(fn (x) x)" ) == "<closure>" );
    assert( run( "(1 2)" ) == "calling a non-function" );
    assert( run( "((fn (x y) x) 1)" ) == "wrong number of arguments" );

    return 0;
}
