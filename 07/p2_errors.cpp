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

/* A type checker is only as good as its error messages. Write a
 * checker for the language of ‹p1_lambda› which, for an ill-typed
 * program, reports «where» the problem is and «what» it is. The
 * result is either the type of the program (written with ‹write›) or
 * a message ‹LINE:COLUMN: TEXT›, where the position (numbered from 1,
 * columns counted in bytes) is that of the first character of the
 * offending sub-expression, and ‹TEXT› is one of:
 *
 *  • ‹unbound variable X› – at the variable,
 *  • ‹expected T, got U› – at a sub-expression which has type ‹U›, but
 *    must have type ‹T›: an operand of an operator, of ‹and›, ‹or› or
 *    ‹not›, the condition of ‹if›, the «second» branch of ‹if› (which
 *    must have the type of the first one), the second operand of ‹=› or
 *    ‹!=› (which must have the type of the first one), or an argument
 *    of a call,
 *  • ‹cannot compare T› – at the first operand of ‹=› or ‹!=›, if its
 *    type ‹T› is a function type,
 *  • ‹not a function: T› – at the callee of a call, which has type ‹T›,
 *  • ‹wrong number of arguments› – at the call,
 *  • ‹bad type› – at a malformed type annotation,
 *  • ‹bad form› – at a list which does not have the shape of any
 *    expression; if the problem is a malformed parameter list of ‹fn›
 *    or a repeated parameter name, at the ‹fn› expression,
 *  • ‹syntax error› – at ‹1:1›, if the program cannot be ‹read›.
 *
 * Types in messages are written with ‹write›. The sub-expressions are
 * checked from left to right (callee first, then the arguments), and
 * only the first error is reported. */

// std::expected< std::string, std::string > check( std::string_view program );

int main()
{
    auto err = []( std::string_view p ) { return check( p ).error(); };

    assert( check( "(fn ((x int)) (< x 1))" ) == "(-> int bool)" );

    assert( err( "(+ 1 #t)" ) == "1:6: expected int, got bool" );
    assert( err( "(let f (fn ((x int)) x)\n  (f #f))" ) == "2:6: expected int, got bool" );
    assert( err( "(if (< 1 2)\n    1\n    (< 1 2))" ) == "3:5: expected int, got bool" );
    assert( err( "(if 1 2 3)" ) == "1:5: expected bool, got int" );
    assert( err( "(+ y 1)" ) == "1:4: unbound variable y" );
    assert( err( "(let g (fn ((h (-> int int))) (h 1 2)) g)" ) == "1:31: wrong number of arguments" );
    assert( err( "(let n 5 (n 1))" ) == "1:11: not a function: int" );
    assert( err( "(= (fn () 1) 2)" ) == "1:4: cannot compare (-> int)" );
    assert( err( "(= 1 #t)" ) == "1:6: expected int, got bool" );
    assert( err( "(fn ((x (-> int float))) x)" ) == "1:9: bad type" );
    assert( err( "(fn ((x int) (x int)) x)" ) == "1:1: bad form" );
    assert( err( "(+ 1 (let x 2))" ) == "1:6: bad form" );
    assert( err( "(not (and #t (fn () #t)))" ) == "1:14: expected bool, got (-> bool)" );
    assert( err( "((fn ((x bool)) x) (+ 1 #f))" ) == "1:25: expected int, got bool" );
    assert( err( "(+ 1 2" ) == "1:1: syntax error" );

    return 0;
}
