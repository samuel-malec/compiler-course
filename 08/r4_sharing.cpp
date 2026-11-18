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

/* The principal type of an expression can be «exponentially» larger
 * than the expression itself. Consider (with ‹pair› as in ‹p4_lists›):
 *
 *     (let f0 (fn (x) (pair x x))
 *       (let f1 (fn (y) (f0 (f0 y)))
 *         (let f2 (fn (y) (f1 (f1 y)))  …
 *
 * The type of ‹(f0 1)› is ‹(pair int int)›, that of ‹(f1 1)› is a pair of
 * pairs of pairs, and every further level doubles the depth. Printing
 * such types is hopeless, but inferring them is not – if they are
 * represented as «DAGs», where identical sub-terms are shared rather
 * than copied: ‹(pair T T)› needs only one copy of ‹T›.
 *
 * Write a function ‹type_size› which infers the type of an expression
 * in the language of ‹p4_lists› (with let-polymorphism, ‹letrec› and
 * the built-ins) and returns its «size»: the number of nodes of the
 * type «written out as a tree» (every atom – ‹int›, ‹bool›, a type
 * variable – counts as 1, and every compound type ‹(c T₁ … Tₙ)› as 1
 * plus the sizes of ‹T₁ … Tₙ›; the constructor name itself does not
 * count). Return ‹std::nullopt› if the expression has no type.
 *
 * The tests contain types with more than ⟦2⁴⁰⟧ nodes. To handle them,
 * the whole inference must work on shared representations: types as
 * nodes referring to other nodes (‹std::shared_ptr›, or indices into a
 * vector), type variables as mutable nodes which get «bound» by
 * unification (a union-find, see ‹r2_unionfind›), instantiation which
 * copies a scheme while preserving its sharing (remember which nodes
 * have already been copied), and a size computation which memoizes
 * the sizes of nodes. Unification of two such types must also avoid
 * comparing the same pair of nodes repeatedly (mark pairs which are
 * already unified – or simply bind the variables as you go, so that
 * a second visit of the same pair finds them identical). */

// std::optional< long > type_size( std::string_view expr );

int main()
{
    assert( type_size( "1" ) == 1 );
    assert( type_size( "(fn (x) x)" ) == 3 );
    assert( type_size( "(pair 1 #t)" ) == 3 );
    assert( type_size( "(cons nil nil)" ) == 3 );
    assert( type_size( "(fn (x) (pair x x))" ) == 5 );
    assert( !type_size( "(+ 1 #t)" ) );

    /* f(k) = fn y => f0 (f(k - 1) y); the type of (f(k) 1) has depth k + 1 */
    auto linear = []( int k )
    {
        std::string s = "(let f0 (fn (x) (pair x x)) ";
        for ( int i = 1; i <= k; ++ i )
            s += "(let f" + std::to_string( i ) + " (fn (y) (f0 (f" + std::to_string( i - 1 ) + " y))) ";
        s += "(f" + std::to_string( k ) + " 1)";
        return s + std::string( k + 1, ')' );
    };

    assert( type_size( linear( 0 ) ) == 3 );
    assert( type_size( linear( 3 ) ) == ( 1L << 5 ) - 1 );
    assert( type_size( linear( 40 ) ) == ( 1L << 42 ) - 1 );

    /* doubling: g(k) = fn y => g(k - 1) (g(k - 1) y), depth 2ᵏ */
    std::string doubling = "(let g0 (fn (x) (pair x x)) "
                           "(let g1 (fn (y) (g0 (g0 y))) "
                           "(let g2 (fn (y) (g1 (g1 y))) "
                           "(let g3 (fn (y) (g2 (g2 y))) "
                           "(let g4 (fn (y) (g3 (g3 y))) "
                           "(let g5 (fn (y) (g4 (g4 y))) "
                           "g5))))))";
    assert( type_size( doubling ) == 2 + ( 1L << 33 ) - 1 );

    return 0;
}
