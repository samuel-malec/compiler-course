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

/* Many languages allow «type aliases» – names for types, like
 * ‹typedef› or ‹using› in C++. An alias is just an abbreviation: two
 * types are equal if they are equal after all aliases are expanded.
 *
 * Write a function ‹equivalent( aliases, a, b )› which decides whether
 * the types ‹a› and ‹b› are equal. Types are as in the introduction,
 * except that an atom may also be the name of an alias, whose
 * definition (a type, which may again use aliases) is given in the
 * map ‹aliases›. Return ‹std::nullopt› if one of the types uses (directly
 * or indirectly) an atom which is neither ‹int›, ‹bool› nor a defined
 * alias, or an alias whose expansion would never end (a cycle). */

using aliases = std::map< std::string, std::string >;

// std::optional< bool > equivalent( const aliases &defs, std::string_view a, std::string_view b );

int main()
{
    aliases defs =
    {
        { "num", "int" },
        { "pred", "(-> num bool)" },
        { "test", "(-> int bool)" },
        { "hof", "(-> pred num num)" },
        { "loop", "(-> loop int)" },
        { "a", "b" }, { "b", "a" },
        { "bad", "(-> int string)" },
    };

    assert( equivalent( defs, "int", "num" ) == true );
    assert( equivalent( defs, "pred", "test" ) == true );
    assert( equivalent( defs, "hof", "(-> (-> int bool) int int)" ) == true );
    assert( equivalent( defs, "hof", "(-> test int bool)" ) == false );
    assert( equivalent( defs, "bool", "num" ) == false );
    assert( equivalent( defs, "(-> num)", "(-> int)" ) == true );

    assert( !equivalent( defs, "loop", "int" ) );
    assert( !equivalent( defs, "a", "int" ) );
    assert( !equivalent( defs, "bad", "bad" ) );
    assert( !equivalent( defs, "int", "float" ) );

    return 0;
}
