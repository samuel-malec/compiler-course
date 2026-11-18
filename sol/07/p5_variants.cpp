#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>

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

struct ill_typed {};

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
bool is_variant( const type &t ) { return t.is_list() && !t.items.empty() && t.items[ 0 ].atom == "variant"; }

/* Validate a type and bring it to canonical form (cases sorted). */
type canonical( const type &t )
{
    if ( t.is_atom() )
    {
        if ( t.atom != "int" && t.atom != "bool" && t.atom != "unit" ) throw ill_typed();
        return t;
    }
    if ( !is_variant( t ) || t.items.size() < 2 ) throw ill_typed();

    std::map< std::string, type > cases;
    for ( std::size_t i = 1; i < t.items.size(); ++ i )
    {
        const auto &c = t.items[ i ];
        if ( !c.is_list() || c.items.size() != 2 || !c.items[ 0 ].is_atom() ||
             !cases.emplace( c.items[ 0 ].atom, canonical( c.items[ 1 ] ) ).second )
            throw ill_typed();
    }

    type out{ "", { atom( "variant" ) }, 0 };
    for ( const auto &[ tag, payload ] : cases )
        out.items.push_back( sexp{ "", { atom( tag ), payload }, 0 } );
    return out;
}

std::map< std::string, type > cases( const type &t )
{
    std::map< std::string, type > out;
    for ( std::size_t i = 1; i < t.items.size(); ++ i )
        out.emplace( t.items[ i ].items[ 0 ].atom, t.items[ i ].items[ 1 ] );
    return out;
}

using context = std::map< std::string, std::string >; /* name → written canonical type */

std::string check( const sexp &e, const context &ctx )
{
    if ( e.is_atom() )
    {
        if ( e.atom == "#t" || e.atom == "#f" ) return "bool";
        if ( e.atom == "unit" ) return "unit";
        if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos ) return "int";
        if ( auto it = ctx.find( e.atom ); it != ctx.end() ) return it->second;
        throw ill_typed();
    }

    const auto &it = e.items;
    if ( it.empty() ) throw ill_typed();
    const std::string &h = it[ 0 ].atom;
    auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw ill_typed(); };
    auto expect = [ & ]( const sexp &x, const std::string &t ) { if ( check( x, ctx ) != t ) throw ill_typed(); };

    if ( h == "+" || h == "-" || h == "*" || h == "/" || h == "<" || h == "<=" || h == ">" || h == ">=" )
    {
        arity( 2 );
        expect( it[ 1 ], "int" );
        expect( it[ 2 ], "int" );
        return ( h == "+" || h == "-" || h == "*" || h == "/" ) ? "int" : "bool";
    }
    if ( h == "=" || h == "!=" )
    {
        arity( 2 );
        std::string t = check( it[ 1 ], ctx );
        if ( t != "int" && t != "bool" ) throw ill_typed();
        expect( it[ 2 ], t );
        return "bool";
    }
    if ( h == "and" || h == "or" || h == "not" )
    {
        arity( h == "not" ? 1 : 2 );
        for ( std::size_t i = 1; i < it.size(); ++ i ) expect( it[ i ], "bool" );
        return "bool";
    }
    if ( h == "if" )
    {
        arity( 3 );
        expect( it[ 1 ], "bool" );
        std::string t = check( it[ 2 ], ctx );
        expect( it[ 3 ], t );
        return t;
    }
    if ( h == "let" )
    {
        arity( 3 );
        context inner = ctx;
        inner[ it[ 1 ].atom ] = check( it[ 2 ], ctx );
        return check( it[ 3 ], inner );
    }
    if ( h == "tag" )
    {
        arity( 3 );
        type t = canonical( it[ 1 ] );
        if ( !is_variant( t ) ) throw ill_typed();
        auto cs = cases( t );
        auto c = cs.find( it[ 2 ].atom );
        if ( c == cs.end() ) throw ill_typed();
        expect( it[ 3 ], write( c->second ) );
        return write( t );
    }
    if ( h == "match" )
    {
        if ( it.size() < 3 ) throw ill_typed();
        type t = canonical( read( check( it[ 1 ], ctx ) ) );
        if ( !is_variant( t ) ) throw ill_typed();
        auto cs = cases( t );
        std::set< std::string > seen;
        std::optional< std::string > result;
        bool has_else = false;

        for ( std::size_t i = 2; i < it.size(); ++ i )
        {
            const auto &c = it[ i ];
            if ( !c.is_list() || c.items.empty() || has_else ) throw ill_typed();
            std::string body;

            if ( c.items[ 0 ].atom == "else" && c.items.size() == 2 )
            {
                has_else = true;
                body = check( c.items[ 1 ], ctx );
            }
            else
            {
                if ( c.items.size() != 3 || !cs.contains( c.items[ 0 ].atom ) ||
                     !seen.insert( c.items[ 0 ].atom ).second )
                    throw ill_typed();
                context inner = ctx;
                inner[ c.items[ 1 ].atom ] = write( cs.at( c.items[ 0 ].atom ) );
                body = check( c.items[ 2 ], inner );
            }

            if ( result && *result != body ) throw ill_typed();
            result = body;
        }

        if ( !has_else && seen.size() != cs.size() ) throw ill_typed();
        return *result;
    }
    throw ill_typed();
}

std::optional< std::string > type_of( std::string_view expr )
{
    try { return check( read( expr ), {} ); }
    catch ( const ill_typed & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

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
