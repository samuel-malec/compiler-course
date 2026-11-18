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

/* Extend ‹p2_poly› with recursive definitions:
 *
 *     (letrec ((f₁ e₁) (f₂ e₂) …) body)
 *
 * where each ‹eᵢ› must be a ‹fn› expression and the names must be
 * distinct. All the names ‹fᵢ› are visible in all the ‹eᵢ› and in the
 * body. While inferring the ‹eᵢ›, each ‹fᵢ› has a single, monomorphic
 * type (a fresh type variable, unified with the type of ‹eᵢ›); only
 * after all the ‹eᵢ› have been inferred are the types generalized (as
 * a group), for use in the body. Return ‹std::nullopt› for type errors
 * and malformed expressions (including a malformed ‹letrec›). */

// std::optional< std::string > infer( std::string_view expr );

int main()
{
    std::string fact = "(letrec ((fact (fn (n) (if (= n 0) 1 (* n (fact (- n 1))))))) ";
    assert( infer( fact + "fact)" ) == "(-> int int)" );
    assert( infer( fact + "(fact 5))" ) == "int" );

    std::string parity = "(letrec ((even (fn (n) (if (= n 0) #t (odd (- n 1)))))"
                         "         (odd (fn (n) (if (= n 0) #f (even (- n 1)))))) ";
    assert( infer( parity + "even)" ) == "(-> int bool)" );

    /* generalized after the group, so ‹rep› can be used at two types */
    std::string rep = "(letrec ((rep (fn (n f x) (if (= n 0) x (rep (- n 1) f (f x)))))) ";
    assert( infer( rep + "rep)" ) == "(-> int (-> 'a 'a) 'a 'a)" );
    assert( infer( rep + "(if (rep 3 (fn (b) (not b)) #t) (rep 2 (fn (y) (* y 2)) 5) 0))" ) == "int" );

    assert( infer( "(letrec ((loop (fn (x) (loop x)))) loop)" ) == "(-> 'a 'b)" );
    assert( infer( "(letrec ((f (fn (x) (if (f #t) x x)))) f)" ) == "(-> bool bool)" );
    assert( infer( "(fn (z) (letrec ((g (fn (n) (if (= n 0) z (g (- n 1)))))) (g 10)))" ) == "(-> 'a 'a)" );

    /* within the group, the type is monomorphic */
    assert( !infer( "(letrec ((g (fn (x) (if (g 1) (g #t) #f)))) g)" ) );
    assert( !infer( "(letrec ((x 1)) x)" ) );
    assert( !infer( "(letrec ((f (fn () 1)) (f (fn () 2))) 1)" ) );
    assert( !infer( "(letrec ((f (fn (x) (f)))) f)" ) );

    return 0;
}
