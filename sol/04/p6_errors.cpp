#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <expected>
#include <map>

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

/* A run-time error message without a position is almost useless in
 * a program of any size. In this exercise, the evaluator must report
 * «where» an error happened. The language:
 *
 *  • integer literals (digits with an optional leading ‹-›) and
 *    variables (any other atom),
 *  • ‹(+ a b)›, ‹(- a b)›, ‹(* a b)› and ‹(/ a b)›,
 *  • ‹(if c t e)› (‹c› is true if it is non-zero),
 *  • ‹(let x v b)›, where ‹x› must be an atom which is not an integer.
 *
 * Write a function ‹eval› which returns either the value of the
 * program, or an error message of the form ‹LINE:COLUMN: MESSAGE›
 * (using ‹std::expected›). Lines and columns are numbered from 1,
 * and columns are counted in bytes; the position is that of the
 * first character of the offending expression. The messages are:
 *
 *  • ‹unbound variable NAME› – at the position of the variable,
 *  • ‹division by zero› – at the position of the ‹(/ …)› expression,
 *  • ‹bad form› – at the position of a list which is not one of the
 *    forms above (including those with a wrong number of elements),
 *  • ‹syntax error› – at position ‹1:1›, if ‹read› fails.
 *
 * Evaluation goes left to right (and only one branch of ‹if› is
 * evaluated), and only the first error is reported. */

// std::expected< long, std::string > eval( std::string_view program );

/* The error carries the offset; it is converted to a line and column
 * only once, at the top. */

struct eval_error
{
    std::size_t offset;
    std::string message;
};

bool is_int( const std::string &a )
{
    std::size_t start = a[ 0 ] == '-' ? 1 : 0;
    return a.size() > start && a.find_first_not_of( "0123456789", start ) == a.npos;
}

long eval( const sexp &e, const std::map< std::string, long > &env )
{
    if ( e.is_atom() )
    {
        if ( is_int( e.atom ) )
            return std::stol( e.atom );
        if ( auto it = env.find( e.atom ); it != env.end() )
            return it->second;
        throw eval_error{ e.offset, "unbound variable " + e.atom };
    }

    const auto &it = e.items;
    auto bad = [ & ] { return eval_error{ e.offset, "bad form" }; };

    if ( it.size() != 4 && it.size() != 3 )
        throw bad();

    const std::string &op = it[ 0 ].atom;

    if ( it.size() == 4 && op == "if" )
        return eval( it[ 1 ], env ) ? eval( it[ 2 ], env ) : eval( it[ 3 ], env );

    if ( it.size() == 4 && op == "let" )
    {
        if ( !it[ 1 ].is_atom() || is_int( it[ 1 ].atom ) )
            throw bad();
        auto inner = env;
        inner[ it[ 1 ].atom ] = eval( it[ 2 ], env );
        return eval( it[ 3 ], inner );
    }

    if ( it.size() != 3 || op.size() != 1 || std::string_view( "+-*/" ).find( op[ 0 ] ) == op.npos )
        throw bad();

    long a = eval( it[ 1 ], env ), b = eval( it[ 2 ], env );

    switch ( op[ 0 ] )
    {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        default:
            if ( b == 0 )
                throw eval_error{ e.offset, "division by zero" };
            return a / b;
    }
}

std::expected< long, std::string > eval( std::string_view program )
{
    sexp e;

    try { e = read( program ); }
    catch ( const std::invalid_argument & ) { return std::unexpected( "1:1: syntax error" ); }

    try
    {
        return eval( e, {} );
    }
    catch ( const eval_error &err )
    {
        int line = 1, col = 1;
        for ( std::size_t i = 0; i < err.offset; ++ i )
            if ( program[ i ] == '\n' )
                ++ line, col = 1;
            else
                ++ col;

        return std::unexpected( std::to_string( line ) + ":" + std::to_string( col ) +
                                ": " + err.message );
    }
}

int main()
{
    auto err = []( std::string_view p ) { return eval( p ).error(); };

    assert( eval( "42" ) == 42 );
    assert( eval( "(let x 6 (* x 7))" ) == 42 );
    assert( eval( "(if 0 (/ 1 0) 5)" ) == 5 );

    assert( err( "x" ) == "1:1: unbound variable x" );
    assert( err( "(+ 1 x)" ) == "1:6: unbound variable x" );
    assert( err( "(+ 1\n   (/ 2 0))" ) == "2:4: division by zero" );
    assert( err( "(let a 1\n  (let b 0\n    (+ a (/ a b))))" ) == "3:10: division by zero" );
    assert( err( "(+ (/ 1 0) y)" ) == "1:4: division by zero" );
    assert( err( "(+ y (/ 1 0))" ) == "1:4: unbound variable y" );
    assert( err( "(if 1 2)" ) == "1:1: bad form" );
    assert( err( "(+ 1 (foo 2 3))" ) == "1:6: bad form" );
    assert( err( "(let 5 1 2)" ) == "1:1: bad form" );
    assert( err( "\n\n  (% 1 2)" ) == "3:3: bad form" );
    assert( err( "(+ 1 2" ) == "1:1: syntax error" );
    assert( err( "(+ 1 2))" ) == "1:1: syntax error" );

    return 0;
}
