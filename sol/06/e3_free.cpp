#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
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

/* Write a function ‹free_vars› which returns the set of «free»
 * variables of an expression in the language from the introduction.
 * A ‹(fn (x …) b)› binds its parameters in ‹b›; ‹(let x e b)› binds ‹x›
 * in ‹b› (but not in ‹e›). Operators and keywords are not variables.
 * You can assume that the input is well formed. (The free variables
 * of a function are exactly what its closure needs to capture – see
 * ‹r2_convert›.) */

// std::set< std::string > free_vars( std::string_view expr );

bool is_keyword( const std::string &h )
{
    static const std::string_view kws[] =
        { "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "=", "!=", "if", "let", "fn" };
    for ( auto k : kws ) if ( h == k ) return true;
    return false;
}

std::set< std::string > free_vars( const sexp &e )
{
    if ( e.is_atom() )
    {
        if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos )
            return {};
        return { e.atom };
    }

    const auto &it = e.items;
    const std::string &head = it[ 0 ].atom;
    std::set< std::string > out;

    if ( head == "fn" )
    {
        out = free_vars( it[ 2 ] );
        for ( const auto &p : it[ 1 ].items )
            out.erase( p.atom );
        return out;
    }

    if ( head == "let" )
    {
        out = free_vars( it[ 3 ] );
        out.erase( it[ 1 ].atom );
        out.merge( free_vars( it[ 2 ] ) );
        return out;
    }

    std::size_t first = it[ 0 ].is_atom() && is_keyword( head ) ? 1 : 0;
    for ( std::size_t i = first; i < it.size(); ++ i )
        out.merge( free_vars( it[ i ] ) );
    return out;
}

std::set< std::string > free_vars( std::string_view expr )
{
    return free_vars( read( expr ) );
}

int main()
{
    using s = std::set< std::string >;

    assert(( free_vars( "1" ) == s{} ));
    assert(( free_vars( "x" ) == s{ "x" } ));
    assert(( free_vars( "(fn (x) x)" ) == s{} ));
    assert(( free_vars( "(fn (x) (+ x y))" ) == s{ "y" } ));
    assert(( free_vars( "(fn (x y) (f x y z))" ) == s{ "f", "z" } ));
    assert(( free_vars( "(let x y (fn (z) (* x z)))" ) == s{ "y" } ));
    assert(( free_vars( "(let x x x)" ) == s{ "x" } ));
    assert(( free_vars( "(if a (fn () b) ((fn (a) a) c))" ) == s{ "a", "b", "c" } ));
    assert(( free_vars( "(fn (f) (fn (x) (f (g x))))" ) == s{ "g" } ));

    return 0;
}
