#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

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
