#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
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

/* Implement the subtyping relation and the «join» (least upper bound)
 * for these types (see ‹d2_subtype›):
 *
 *     type = 'int' | 'bool' | 'top'
 *          | (record (field type) …)       ; fields in any order
 *          | (-> type … type) ;
 *
 * Subtyping: every type is a subtype of itself and of ‹top›; records
 * have width and depth subtyping; functions are contravariant in the
 * parameter types and covariant in the result type (and the number of
 * parameters must be the same).
 *
 * The join ‹join( a, b )› is the type written in «canonical form»,
 * where the fields of every record are listed in alphabetical order:
 *
 *  • if ‹a <: b›, the join is ‹b›; if ‹b <: a›, it is ‹a›,
 *  • otherwise, if both are records, the join is a record with the
 *    fields that ‹a› and ‹b› have in common, the type of each being the
 *    join of its types in ‹a› and ‹b›,
 *  • otherwise, if both are functions with the same number of
 *    parameters whose types are pairwise «equivalent» (each a subtype
 *    of the other), the join is a function with the same parameter
 *    types (taken from ‹a›) and the join of the result types,
 *  • otherwise, the join is ‹top›.
 *
 * You can assume that the inputs are well-formed types, and that no
 * record has two fields with the same name. */

// bool subtype( std::string_view a, std::string_view b );
// std::string join( std::string_view a, std::string_view b );

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
bool is( const type &t, std::string_view h ) { return t.is_list() && !t.items.empty() && t.items[ 0 ].atom == h; }

std::map< std::string, type > fields( const type &r )
{
    std::map< std::string, type > out;
    for ( std::size_t i = 1; i < r.items.size(); ++ i )
        out.emplace( r.items[ i ].items[ 0 ].atom, r.items[ i ].items[ 1 ] );
    return out;
}

bool subtype( const type &s, const type &t )
{
    if ( t.is_atom() && t.atom == "top" ) return true;
    if ( s.is_atom() || t.is_atom() ) return s.is_atom() && t.is_atom() && s.atom == t.atom;

    if ( is( s, "record" ) && is( t, "record" ) )
    {
        auto fs = fields( s );
        for ( const auto &[ n, ft ] : fields( t ) )
            if ( !fs.contains( n ) || !subtype( fs.at( n ), ft ) )
                return false;
        return true;
    }

    if ( is( s, "->" ) && is( t, "->" ) && s.items.size() == t.items.size() )
    {
        for ( std::size_t i = 1; i + 1 < s.items.size(); ++ i )
            if ( !subtype( t.items[ i ], s.items[ i ] ) )
                return false;
        return subtype( s.items.back(), t.items.back() );
    }

    return false;
}

/* Sort the fields of all records (recursively). */
type canonical( const type &t )
{
    if ( t.is_atom() ) return t;
    type out{ "", { t.items[ 0 ] }, 0 };
    if ( is( t, "record" ) )
        for ( const auto &[ n, ft ] : fields( t ) )
            out.items.push_back( sexp{ "", { atom( n ), canonical( ft ) }, 0 } );
    else
        for ( std::size_t i = 1; i < t.items.size(); ++ i )
            out.items.push_back( canonical( t.items[ i ] ) );
    return out;
}

type join( const type &a, const type &b )
{
    if ( subtype( a, b ) ) return canonical( b );
    if ( subtype( b, a ) ) return canonical( a );

    if ( is( a, "record" ) && is( b, "record" ) )
    {
        type out{ "", { atom( "record" ) }, 0 };
        auto fb = fields( b );
        for ( const auto &[ n, ft ] : fields( a ) )
            if ( fb.contains( n ) )
                out.items.push_back( sexp{ "", { atom( n ), join( ft, fb.at( n ) ) }, 0 } );
        return out;
    }

    if ( is( a, "->" ) && is( b, "->" ) && a.items.size() == b.items.size() )
    {
        type out{ "", { atom( "->" ) }, 0 };
        for ( std::size_t i = 1; i + 1 < a.items.size(); ++ i )
        {
            if ( !subtype( a.items[ i ], b.items[ i ] ) || !subtype( b.items[ i ], a.items[ i ] ) )
                return atom( "top" );
            out.items.push_back( canonical( a.items[ i ] ) );
        }
        out.items.push_back( join( a.items.back(), b.items.back() ) );
        return out;
    }

    return atom( "top" );
}

bool subtype( std::string_view a, std::string_view b ) { return subtype( read( a ), read( b ) ); }
std::string join( std::string_view a, std::string_view b ) { return write( join( read( a ), read( b ) ) ); }

int main()
{
    assert( subtype( "int", "int" ) );
    assert( subtype( "bool", "top" ) );
    assert( !subtype( "int", "bool" ) );
    assert( !subtype( "top", "int" ) );
    assert( subtype( "(record (x int) (y bool))", "(record (y bool))" ) );
    assert( subtype( "(record (x int) (y bool))", "(record (y top) (x int))" ) );
    assert( subtype( "(record)", "(record)" ) );
    assert( !subtype( "(record (x int))", "(record (x bool))" ) );
    assert( subtype( "(-> top int)", "(-> int top)" ) );
    assert( !subtype( "(-> int int)", "(-> top int)" ) );
    assert( !subtype( "(-> int int)", "(-> int int int)" ) );
    assert( subtype( "(-> (-> int top) (record (a int) (b int)))", "(-> (-> top int) (record (a int)))" ) );

    assert( join( "int", "int" ) == "int" );
    assert( join( "int", "bool" ) == "top" );
    assert( join( "(record (y int) (x int))", "(record (x int))" ) == "(record (x int))" );
    assert( join( "(record (x int))", "(record (y int) (x int))" ) == "(record (x int))" );
    assert( join( "(record (y int) (x int))", "(record (z int) (y bool))" ) == "(record (y top))" );
    assert( join( "(record (b int) (a (record (p int) (q int))))",
                  "(record (a (record (q int) (r int))) (b int))" ) == "(record (a (record (q int))) (b int))" );
    assert( join( "(-> int (record (x int) (y int)))", "(-> int (record (x int) (z int)))" ) ==
            "(-> int (record (x int)))" );
    assert( join( "(-> (record (y int) (x int)) int)", "(-> (record (x int) (y int)) bool)" ) ==
            "(-> (record (x int) (y int)) top)" );
    assert( join( "(-> int int)", "(-> bool int)" ) == "top" );
    assert( join( "(-> int int)", "int" ) == "top" );

    return 0;
}
