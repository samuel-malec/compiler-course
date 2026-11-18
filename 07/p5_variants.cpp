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

/* «Sum types» (variants, tagged unions, ‹std::variant›) describe values
 * which are one of several alternatives. We write them as
 *
 *     (variant (TAG₁ T₁) (TAG₂ T₂) …)
 *
 * – a value of this type is one of the tags, together with a
 * «payload» of the corresponding type. (The order of the cases does
 * not matter for type equality.) Payload types can be ‹int›, ‹bool›,
 * ‹unit› (a type with the single value ‹unit›, for tags which carry no
 * information) or other variant types. Add these expressions to the
 * typed language from the introduction (without functions):
 *
 *  • ‹unit› – the value of type ‹unit›,
 *  • ‹(tag T NAME e)› – build a value of the variant type ‹T›, which
 *    must have a case ‹NAME› whose payload type is the type of ‹e›; the
 *    type of the expression is ‹T›,
 *  • ‹(match e (NAME₁ x₁ b₁) … [(else b)])› – ‹e› must have a variant
 *    type; each case names one of its tags (at most once), and is
 *    checked with ‹xᵢ› bound to the payload type of that tag; the
 *    optional ‹else› case, which must be last, handles the remaining
 *    tags; the cases must be «exhaustive»: without ‹else›, all tags
 *    must be covered; all the bodies must have the same type, which
 *    is the type of the ‹match›.
 *
 * Write ‹type_of›, which returns the type of an expression, or
 * ‹std::nullopt› if it is ill-typed or malformed. Variant types in the
 * result are written in «canonical form», with the cases sorted by tag
 * name. A variant type with two cases with the same tag (or no cases at
 * all) is not a valid type. */

// std::optional< std::string > type_of( std::string_view expr );

int main()
{
    const std::string opt = "(variant (None unit) (Some int))";
    const std::string shape = "(variant (Circle int) (Square int) (Dot unit))";

    assert( type_of( "(tag " + opt + " Some 5)" ) == opt );
    assert( type_of( "(tag (variant (Some int) (None unit)) None unit)" ) == opt );
    assert( type_of( "(let x (tag " + opt + " Some 5) (match x (Some n (+ n 1)) (None u 0)))" ) == "int" );
    assert( type_of( "(match (tag " + shape + " Dot unit) (Circle r (* 3 (* r r))) (Square s (* s s)) (Dot u 0))" ) ==
            "int" );
    assert( type_of( "(match (tag " + shape + " Dot unit) (Circle r #t) (else #f))" ) == "bool" );
    assert( type_of( "(let o (tag " + opt + " None unit)"
                     " (match o (None u (tag " + opt + " Some 0)) (Some n (tag (variant (Some int) (None unit)) Some n))))" ) ==
            opt );
    assert( type_of( "(tag (variant (L int) (R (variant (A bool) (B int)))) R (tag (variant (B int) (A bool)) A #t))" ) ==
            "(variant (L int) (R (variant (A bool) (B int))))" );

    assert( !type_of( "(tag " + opt + " Some #t)" ) );
    assert( !type_of( "(tag " + opt + " Other 1)" ) );
    assert( !type_of( "(tag int Some 1)" ) );
    assert( !type_of( "(tag (variant (A int) (A bool)) A 1)" ) );
    assert( !type_of( "(match (tag " + opt + " Some 5) (Some n n))" ) );                /* not exhaustive */
    assert( !type_of( "(match (tag " + opt + " Some 5) (Some n n) (Some m m) (None u 0))" ) );
    assert( !type_of( "(match (tag " + opt + " Some 5) (Some n n) (None u #f))" ) );
    assert( !type_of( "(match (tag " + opt + " Some 5) (else 1) (Some n n))" ) );
    assert( !type_of( "(match 5 (else 1))" ) );
    assert( !type_of( "(match (tag " + opt + " Some 5) (Some n n) (Nope u 0) (None u 1))" ) );

    return 0;
}
