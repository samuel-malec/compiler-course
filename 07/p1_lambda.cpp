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

/* Write a type checker for the complete typed language from the
 * introduction, including functions and calls. The function
 * ‹type_of› returns the type of the expression (written with
 * ‹write›), or ‹std::nullopt› if the expression is ill-typed or
 * malformed. In particular, the following are errors:
 *
 *  • a parameter annotation which is not a valid type (a valid type is
 *    ‹int›, ‹bool›, or ‹(-> σ₁ … σₙ τ)› with valid ‹σᵢ› and ‹τ›),
 *  • two parameters of the same function with the same name,
 *  • comparing (with ‹=› or ‹!=›) two values of a function type,
 *  • calling something which is not a function, or with a wrong
 *    number of arguments, or with an argument of a wrong type. */

// std::optional< std::string > type_of( std::string_view expr );

int main()
{
    assert( type_of( "(fn ((x int)) (+ x 1))" ) == "(-> int int)" );
    assert( type_of( "(fn () #t)" ) == "(-> bool)" );
    assert( type_of( "((fn ((x int) (y bool)) (if y x 0)) 5 #t)" ) == "int" );
    assert( type_of( "(fn ((f (-> int int))) (fn ((x int)) (f (f x))))" ) ==
            "(-> (-> int int) (-> int int))" );
    assert( type_of( "(let compose (fn ((f (-> int bool)) (g (-> int int)))"
                     " (fn ((x int)) (f (g x)))) (compose (fn ((n int)) (< n 0)) (fn ((n int)) (- 0 n))))" ) ==
            "(-> int bool)" );
    assert( type_of( "(let x 1 (fn ((x bool)) x))" ) == "(-> bool bool)" );
    assert( type_of( "((fn ((f (-> int))) (f)) (fn () 7))" ) == "int" );
    assert( type_of( "(if #t (fn ((x int)) x) (fn ((y int)) (* y 2)))" ) == "(-> int int)" );

    assert( !type_of( "(fn ((x int)) (x 1))" ) );
    assert( !type_of( "((fn ((x int)) x) #t)" ) );
    assert( !type_of( "((fn ((x int)) x) 1 2)" ) );
    assert( !type_of( "(fn ((x int) (x bool)) x)" ) );
    assert( !type_of( "(fn ((x float)) x)" ) );
    assert( !type_of( "(fn ((x (->))) x)" ) );
    assert( !type_of( "(= (fn () 1) (fn () 1))" ) );
    assert( !type_of( "(if #t (fn ((x int)) x) (fn ((x bool)) x))" ) );
    assert( !type_of( "(fn (x) x)" ) );

    return 0;
}
