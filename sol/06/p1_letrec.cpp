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

/* Extend the evaluator from ‹e1_apply› with recursive definitions:
 *
 *     (letrec ((f₁ e₁) (f₂ e₂) …) body)
 *
 * Each ‹eᵢ› must be a function expression ‹(fn …)› (check this before
 * evaluating anything). All the names ‹fᵢ› are visible in all the
 * ‹eᵢ› and in ‹body›, so the functions can call themselves and each
 * other. As before, ‹eval› returns the integer value of the program,
 * or ‹std::nullopt› on any error (see ‹e1_apply›), including a
 * malformed ‹letrec› or a duplicate name in one ‹letrec›. See
 * ‹d1_closures› for the idea, and the introduction for a note on
 * memory management. */

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
        if ( head == "letrec" )
        {
            if ( it.size() != 3 || !it[ 1 ].is_list() ) throw bad();
            frame *f = new_frame( env );
            for ( const auto &b : it[ 1 ].items )
            {
                if ( !b.is_list() || b.items.size() != 2 || !b.items[ 0 ].is_atom() ||
                     !b.items[ 1 ].is_list() || b.items[ 1 ].items.empty() ||
                     b.items[ 1 ].items[ 0 ].atom != "fn" ||
                     !f->vars.emplace( b.items[ 0 ].atom, 0L ).second )
                    throw bad();
            }
            for ( const auto &b : it[ 1 ].items )
                f->vars[ b.items[ 0 ].atom ] = eval( b.items[ 1 ], f );
            return eval( it[ 2 ], f );
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
    assert( eval( "(letrec ((fact (fn (n) (if (= n 0) 1 (* n (fact (- n 1))))))) (fact 10))" ) == 3628800 );
    assert( eval( "(letrec ((fib (fn (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2))))))) (fib 15))" ) == 610 );
    assert( eval( "(letrec ((even (fn (n) (if (= n 0) 1 (odd (- n 1)))))"
                  "         (odd (fn (n) (if (= n 0) 0 (even (- n 1))))))"
                  " (+ (* 10 (even 10)) (odd 7)))" ) == 11 );
    assert( eval( "(letrec ((ack (fn (m n) (if (= m 0) (+ n 1)"
                  "   (if (= n 0) (ack (- m 1) 1) (ack (- m 1) (ack m (- n 1))))))))"
                  " (ack 2 3))" ) == 9 );

    /* a recursive function which returns a closure */
    assert( eval( "(letrec ((adder (fn (n) (if (= n 0) (fn (x) x)"
                  "   (let r (adder (- n 1)) (fn (x) (+ 1 (r x))))))))"
                  " ((adder 5) 10))" ) == 15 );

    /* a letrec function used outside of the letrec */
    assert( eval( "(let sq (letrec ((sq (fn (n) (* n n)))) sq) (sq 9))" ) == 81 );

    /* the inner letrec shadows the outer f */
    assert( eval( "(letrec ((f (fn (x) 1))) (+ (f 0) (letrec ((f (fn (x) (if x (f 0) 10)))) (f 1))))" ) == 11 );

    assert( !eval( "(letrec ((x 1)) x)" ) );
    assert( !eval( "(letrec ((f (fn () 1)) (f (fn () 2))) (f))" ) );
    assert( !eval( "(letrec (f (fn () 1)) (f))" ) );
    assert( !eval( "(letrec ((f (fn (n) (g n)))) (f 1))" ) );

    return 0;
}
