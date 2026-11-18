#include <cassert>
#include <map>
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

/* Write a function ‹generalize( ctx, t )› which turns the type ‹t›
 * into a «type scheme», by quantifying all type variables which occur
 * in ‹t› but do «not» occur in any of the types in the context ‹ctx›
 * (a map from names to monomorphic types). The result is written as
 * ‹(forall ('x 'y …) T)›, with the quantified variables listed in the
 * order of their first appearance in ‹t›, and with the original names
 * (no normalization). If there is nothing to quantify, the result is
 * just ‹T›. */

// std::string generalize( const std::map< std::string, std::string > &ctx, std::string_view t );

int main()
{
    std::map< std::string, std::string > ctx{ { "x", "'p" }, { "f", "(-> 'q int)" } };

    assert( generalize( {}, "int" ) == "int" );
    assert( generalize( {}, "(-> 'z 'z)" ) == "(forall ('z) (-> 'z 'z))" );
    assert( generalize( {}, "(-> 'b 'a 'b)" ) == "(forall ('b 'a) (-> 'b 'a 'b))" );
    assert( generalize( ctx, "(-> 'p 'r)" ) == "(forall ('r) (-> 'p 'r))" );
    assert( generalize( ctx, "(-> 'q 'p)" ) == "(-> 'q 'p)" );
    assert( generalize( ctx, "(-> 's (-> 'q 's) 't)" ) == "(forall ('s 't) (-> 's (-> 'q 's) 't))" );

    return 0;
}
