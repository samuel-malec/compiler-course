#include <cassert>
#include <map>
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

/* This demo adds «records» and «subtyping» to the typed language.
 * Record types are written ‹(record (f₁ τ₁) (f₂ τ₂) …)›, the order of
 * the fields does not matter. There is also a type ‹top›, which is a
 * supertype of every type. The new expressions are:
 *
 *  • ‹(rec (f₁ e₁) …)› – build a record; its type lists all the fields,
 *  • ‹(get e f)› – the field ‹f› of the record ‹e›.
 *
 * The idea of subtyping is «substitutability»: ‹σ <: τ› means that a
 * value of type ‹σ› can be used wherever a ‹τ› is expected. The rules:
 *
 *  • every type is a subtype of itself and of ‹top›,
 *  • «width»: a record with «more» fields is a subtype of one with
 *    fewer (the extra fields are simply ignored), and «depth»: field
 *    types may be subtypes (‹(record (p (record (x int) (y int))))› is
 *    a subtype of ‹(record (p (record (x int))))›),
 *  • functions: ‹(-> σ₁ τ₁) <: (-> σ₂ τ₂)› if ‹σ₂ <: σ₁› and ‹τ₁ <: τ₂›.
 *
 * The function rule deserves a moment of thought. A function is used
 * by calling it: if we expect a ‹(-> σ₂ τ₂)›, we will pass it a ‹σ₂›
 * and use the result as a ‹τ₂›. A function of type ‹(-> σ₁ τ₁)› is a
 * fine replacement if it «accepts» everything we might pass (‹σ₂ <:
 * σ₁› – it can be more liberal in what it takes) and «returns»
 * something we can use (‹τ₁ <: τ₂› – it can be more specific in what it
 * gives back). Parameter types are «contravariant».
 *
 * In the type checker, subtyping shows up in exactly one place – the
 * rule of «subsumption»: wherever an expression of type ‹τ› is
 * expected (an argument of a call), one of any subtype ‹σ <: τ› is
 * fine. And ‹if› needs the «join» (least upper bound) of the types of
 * its branches. */

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
bool is( const type &t, std::string_view head ) { return t.is_list() && t.items.at( 0 ).atom == head; }

struct type_error : std::runtime_error { using std::runtime_error::runtime_error; };

/* The fields of a record type, as a map (so the order does not matter). */

std::map< std::string, type > fields( const type &r )
{
    std::map< std::string, type > out;
    for ( std::size_t i = 1; i < r.items.size(); ++ i )
        out.emplace( r.items[ i ].items.at( 0 ).atom, r.items[ i ].items.at( 1 ) );
    return out;
}

bool subtype( const type &s, const type &t )
{
    if ( t.is_atom() && t.atom == "top" )
        return true;
    if ( s.is_atom() || t.is_atom() )
        return s.is_atom() && t.is_atom() && s.atom == t.atom;

    if ( is( s, "record" ) && is( t, "record" ) )
    {
        auto fs = fields( s );
        for ( const auto &[ name, ft ] : fields( t ) )   /* every field of ‹t› … */
            if ( !fs.contains( name ) || !subtype( fs.at( name ), ft ) ) /* … must be in ‹s› */
                return false;
        return true;
    }

    if ( is( s, "->" ) && is( t, "->" ) && s.items.size() == t.items.size() )
    {
        for ( std::size_t i = 1; i + 1 < s.items.size(); ++ i )
            if ( !subtype( t.items[ i ], s.items[ i ] ) ) /* contravariant */
                return false;
        return subtype( s.items.back(), t.items.back() ); /* covariant */
    }

    return false;
}

/* The join of two types: the smallest common supertype. For records,
 * it keeps the fields they have in common (with joined types). To
 * keep the demo short, two function types only have a join if they
 * are equal; otherwise (and for unrelated types), the join is ‹top›. */

type join( const type &a, const type &b )
{
    if ( subtype( a, b ) ) return b;
    if ( subtype( b, a ) ) return a;

    if ( is( a, "record" ) && is( b, "record" ) )
    {
        type out{ "", { atom( "record" ) }, 0 };
        auto fb = fields( b );
        for ( const auto &[ name, ft ] : fields( a ) )
            if ( fb.contains( name ) )
                out.items.push_back( sexp{ "", { atom( name ), join( ft, fb.at( name ) ) }, 0 } );
        return out;
    }

    return atom( "top" );
}

using context = std::map< std::string, type >;

type check( const sexp &e, const context &ctx )
{
    if ( e.is_atom() )
    {
        if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos ) return atom( "int" );
        if ( e.atom == "#t" || e.atom == "#f" ) return atom( "bool" );
        if ( auto it = ctx.find( e.atom ); it != ctx.end() ) return it->second;
        throw type_error( "unbound variable " + e.atom );
    }

    const auto &it = e.items;
    const std::string &head = it.at( 0 ).atom;

    if ( head == "rec" )
    {
        type out{ "", { atom( "record" ) }, 0 };
        for ( std::size_t i = 1; i < it.size(); ++ i )
            out.items.push_back( sexp{ "", { it[ i ].items.at( 0 ), check( it[ i ].items.at( 1 ), ctx ) }, 0 } );
        return out;
    }

    if ( head == "get" )
    {
        type r = check( it.at( 1 ), ctx );
        if ( !is( r, "record" ) || !fields( r ).contains( it.at( 2 ).atom ) )
            throw type_error( "no field " + it[ 2 ].atom + " in " + write( r ) );
        return fields( r ).at( it[ 2 ].atom );
    }

    if ( head == "if" )
    {
        if ( write( check( it.at( 1 ), ctx ) ) != "bool" )
            throw type_error( "condition must be bool" );
        return join( check( it.at( 2 ), ctx ), check( it.at( 3 ), ctx ) );
    }

    if ( head == "fn" )
    {
        context inner = ctx;
        type out{ "", { atom( "->" ) }, 0 };
        for ( const auto &p : it.at( 1 ).items )
        {
            inner.insert_or_assign( p.items.at( 0 ).atom, p.items.at( 1 ) );
            out.items.push_back( p.items[ 1 ] );
        }
        out.items.push_back( check( it.at( 2 ), inner ) );
        return out;
    }

    /* a call, with subsumption for the arguments */
    type f = check( it[ 0 ], ctx );
    if ( !is( f, "->" ) || f.items.size() != it.size() + 1 )
        throw type_error( "bad call" );
    for ( std::size_t i = 1; i < it.size(); ++ i )
        if ( type a = check( it[ i ], ctx ); !subtype( a, f.items[ i ] ) )
            throw type_error( write( a ) + " is not a subtype of " + write( f.items[ i ] ) );
    return f.items.back();
}

bool sub( std::string_view a, std::string_view b ) { return subtype( read( a ), read( b ) ); }
std::string type_of( std::string_view e )
{
    try { return write( check( read( e ), {} ) ); }
    catch ( const type_error &err ) { return err.what(); }
}

int main() /* demo */
{
    assert( sub( "(record (x int) (y int))", "(record (x int))" ) );       /* width */
    assert( !sub( "(record (x int))", "(record (x int) (y int))" ) );
    assert( sub( "(record (y bool) (x int))", "(record (x int) (y bool))" ) ); /* order */
    assert( sub( "(record (p (record (x int) (y int))))", "(record (p (record (x int))))" ) ); /* depth */
    assert( sub( "int", "top" ) && !sub( "top", "int" ) );

    /* functions: contravariant in the parameter, covariant in the result */
    assert( sub( "(-> (record (x int)) (record (x int) (y int)))",
                 "(-> (record (x int) (y int)) (record (x int)))" ) );
    assert( !sub( "(-> (record (x int) (y int)) int)", "(-> (record (x int)) int)" ) );

    /* a function which needs only ‹x› accepts a record with more fields */
    assert( type_of( "((fn ((p (record (x int)))) (get p x)) (rec (x 1) (y #t)))" ) == "int" );
    assert( type_of( "((fn ((p (record (x int) (y int)))) (get p y)) (rec (x 1)))" ) ==
            "(record (x int)) is not a subtype of (record (x int) (y int))" );

    /* the join keeps the common fields */
    assert( type_of( "(if #t (rec (x 1) (y 2)) (rec (x 3) (z #f)))" ) == "(record (x int))" );
    assert( type_of( "(if #t 1 #f)" ) == "top" ); /* accepted now, but not very useful */
    assert( type_of( "(get (if #t (rec (x 1) (y 2)) (rec (x 3))) y)" ) == "no field y in (record (x int))" );

    return 0;
}
