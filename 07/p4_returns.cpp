#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

/* In this exercise, we combine the statements of Chapter 5 with
 * types. A program is a list of function definitions:
 *
 *     (program (fun NAME ((x T) …) R BODY) …)
 *
 * where the types ‹T› and ‹R› are ‹int› or ‹bool›, and ‹BODY› is a
 * statement:
 *
 *     stmt = (block stmt …) | (var x expr) | (set x expr)
 *          | (if expr stmt [stmt]) | (while expr stmt)
 *          | (return expr) | (do expr) ;
 *
 * Expressions are those of the typed language from the introduction,
 * without ‹let›, ‹fn› and function values: a call ‹(NAME a …)› calls
 * one of the functions of the program (they can call each other in
 * any order, and recursively). ‹(var x e)› declares ‹x› with the type
 * of ‹e›; ‹(set x e)› requires that ‹e› has the type of ‹x›; ‹(do e)›
 * evaluates an expression for its effect; ‹(return e)› requires that
 * ‹e› has the declared result type ‹R›. Scoping is as in ‹05/p2_resolve›
 * (blocks and the bodies of ‹if› and ‹while› open scopes); the
 * parameters are in a scope enclosing the body.
 *
 * A statement «always returns» if it is a ‹return›; an ‹if› «with»
 * ‹else› whose both branches always return; or a ‹block› containing a
 * statement which always returns. Nothing else always returns (not
 * even ‹(while #t …)›).
 *
 * Write a function ‹check› which returns the list of errors, one per
 * function at most, in the order of the definitions, each of the form
 * ‹NAME: MESSAGE›, where ‹MESSAGE› describes the «first» problem found
 * when checking the body from left to right:
 *
 *  • ‹unreachable code› – a statement in a ‹block› follows a statement
 *    which always returns,
 *  • ‹type error› – any problem with types or names (including calls of
 *    unknown functions, wrong argument types or counts, bad annotations
 *    and returning a value of the wrong type),
 *  • ‹missing return› – the body (checked without errors) does not
 *    always return.
 *
 * If two functions have the same name, the result is just the single
 * error ‹duplicate function NAME› (for the first duplicate). An empty
 * result means that the program is fine. You can assume that the
 * program is well formed. */

// std::vector< std::string > check( std::string_view program );

int main()
{
    using v = std::vector< std::string >;

    assert(( check( "(program (fun f ((x int)) int (return (* x 2))))" ) == v{} ));

    assert(( check( "(program"
                    " (fun abs ((x int)) int (if (< x 0) (return (- 0 x)) (return x)))"
                    " (fun even ((n int)) bool (if (= n 0) (return #t) (return (odd (- n 1)))))"
                    " (fun odd ((n int)) bool (block (if (= n 0) (return #f)) (return (even (- n 1)))))"
                    " (fun sum ((n int)) int (block (var s 0)"
                    "   (while (> n 0) (block (set s (+ s n)) (set n (- n 1)))) (return s))))" ) == v{} ));

    assert(( check( "(program (fun f ((x int)) int (if (< x 0) (return 0))))" ) ==
             v{ "f: missing return" } ));
    assert(( check( "(program (fun f () int (while #t (return 1))))" ) == v{ "f: missing return" } ));
    assert(( check( "(program (fun f () int (block (return 1) (do 2))))" ) == v{ "f: unreachable code" } ));
    assert(( check( "(program (fun f () int (block (if #t (return 1) (return 2)) (return 3))))" ) ==
             v{ "f: unreachable code" } ));
    assert(( check( "(program (fun f () int (return #t)))" ) == v{ "f: type error" } ));
    assert(( check( "(program (fun f () int (block (var x 1) (set x #f) (return x))))" ) == v{ "f: type error" } ));
    assert(( check( "(program (fun f () int (return (g 1))))" ) == v{ "f: type error" } ));
    assert(( check( "(program (fun f ((b bool)) int (return 1)) (fun g () int (return (f 1))))" ) ==
             v{ "g: type error" } ));
    assert(( check( "(program (fun f () int (block (if #t (var y 1)) (return y))))" ) == v{ "f: type error" } ));
    assert(( check( "(program (fun f () int (block (do (+ 1 #t)) (return 1) (return 2))))" ) ==
             v{ "f: type error" } ));
    assert(( check( "(program (fun f () int (block)) (fun g () bool (return 1)) (fun h () int (return 0)))" ) ==
             v{ "f: missing return", "g: type error" } ));
    assert(( check( "(program (fun f () int (return 1)) (fun f () bool (return #t)))" ) ==
             v{ "duplicate function f" } ));

    return 0;
}
