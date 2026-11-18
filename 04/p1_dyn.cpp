#include <cassert>
#include <optional>
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

/* Write an evaluator for a small dynamically typed language, whose
 * values are integers, booleans and strings: */

using value = std::variant< long, bool, std::string >;

/* The expressions are:
 *
 *  • an atom which consists of digits, optionally preceded by ‹-›, is
 *    an integer literal; ‹#t› and ‹#f› are the boolean literals; an
 *    atom which starts with ‹"› is a string literal (its value does
 *    not include the quotes); any other atom is a variable,
 *  • ‹(+ a b)›, ‹(- a b)›, ‹(* a b)› and ‹(/ a b)› – arithmetic on
 *    integers (division truncates towards zero),
 *  • ‹(++ a b)› – concatenation of two strings, ‹(len a)› – the length
 *    of a string,
 *  • ‹(< a b)› – comparison of two integers or two strings (strings
 *    are compared lexicographically, as by ‹<› on ‹std::string›),
 *  • ‹(= a b)› – true iff ‹a› and ‹b› are of the same type and equal;
 *    unlike all other operators, it accepts operands of any type,
 *  • ‹(not a)›, ‹(and a b)›, ‹(or a b)› – boolean operators; ‹and›
 *    and ‹or› are «short-circuiting»: they evaluate ‹b› only if the
 *    value of ‹a› does not already decide the result,
 *  • ‹(if c t e)› – ‹c› must be a boolean; only one branch is
 *    evaluated,
 *  • ‹(let x v b)› – evaluate ‹v›, then ‹b› with ‹x› bound to the
 *    value of ‹v›.
 *
 * Write a function ‹eval( program )› which reads the program (using
 * ‹read›) and evaluates it in an empty environment. Return
 * ‹std::nullopt› if the program is malformed (cannot be read, has an
 * unknown operator, a wrong number of operands, …) or if evaluation
 * fails: an operand of the wrong type, division by zero or an unbound
 * variable. Only errors which actually happen count: an ill-typed
 * expression in a branch which is not taken is fine. */

// std::optional< value > eval( std::string_view program );

int main()
{
    using namespace std::literals;

    assert( eval( "42" ) == value( 42L ) );
    assert( eval( "-7" ) == value( -7L ) );
    assert( eval( "#t" ) == value( true ) );
    assert( eval( "\"hi\"" ) == value( "hi"s ) );
    assert( eval( "(+ 1 (* 2 3))" ) == value( 7L ) );
    assert( eval( "(/ -7 2)" ) == value( -3L ) );
    assert( eval( "(++ \"ab\" \"cd\")" ) == value( "abcd"s ) );
    assert( eval( "(len (++ \"ab\" \"cd\"))" ) == value( 4L ) );
    assert( eval( "(< 1 2)" ) == value( true ) );
    assert( eval( "(< \"b\" \"ab\")" ) == value( false ) );
    assert( eval( "(= 1 1)" ) == value( true ) );
    assert( eval( "(= 1 #t)" ) == value( false ) );
    assert( eval( "(= \"a\" \"a\")" ) == value( true ) );
    assert( eval( "(not (< 2 1))" ) == value( true ) );
    assert( eval( "(and #f (/ 1 0))" ) == value( false ) );
    assert( eval( "(or #t (+ 1 \"x\"))" ) == value( true ) );
    assert( eval( "(and #t #f)" ) == value( false ) );
    assert( eval( "(if (< 1 2) \"yes\" (/ 1 0))" ) == value( "yes"s ) );
    assert( eval( "(let x 5 (let y (* x 2) (+ x y)))" ) == value( 15L ) );
    assert( eval( "(let x 1 (+ (let x 2 x) x))" ) == value( 3L ) );
    assert( eval( "(let s \"ab\" (if (= (len s) 2) (++ s s) s))" ) == value( "abab"s ) );

    assert( !eval( "x" ) );
    assert( !eval( "(+ 1 #t)" ) );
    assert( !eval( "(++ \"a\" 1)" ) );
    assert( !eval( "(< 1 \"a\")" ) );
    assert( !eval( "(/ 1 0)" ) );
    assert( !eval( "(if 1 2 3)" ) );
    assert( !eval( "(and 1 #t)" ) );
    assert( !eval( "(and #t 1)" ) );
    assert( !eval( "(not 0)" ) );
    assert( !eval( "(+ 1)" ) );
    assert( !eval( "(+ 1 2 3)" ) );
    assert( !eval( "(foo 1 2)" ) );
    assert( !eval( "()" ) );
    assert( !eval( "(let 1 2 3)" ) );
    assert( !eval( "(+ 1 2" ) );

    return 0;
}
