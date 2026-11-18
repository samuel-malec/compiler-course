#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
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

/* C++, Java and many other languages allow «overloading»: several
 * functions with the same name, distinguished by the types of their
 * parameters. The type checker has to decide which one a call refers
 * to – this is «overload resolution». In this exercise, we implement a
 * simplified version of the C++ rules.
 *
 * The types are ‹int›, ‹float› and ‹string›, and there is one «implicit
 * conversion»: from ‹int› to ‹float›. An expression is a literal – an
 * integer (digits, with an optional ‹-›) of type ‹int›, a number with a
 * decimal point (like ‹1.5›) of type ‹float›, or a string literal (in
 * double quotes) of type ‹string› – or a call ‹(NAME arg …)›. The
 * available functions are given as a list of «signatures», strings of
 * the form ‹NAME (-> T₁ … Tₙ R)›.
 *
 * To resolve a call ‹(f a₁ … aₙ)›, first find the types of the
 * arguments (recursively). Then:
 *
 *  1. The «candidates» are the signatures for ‹f› with ‹n› parameters.
 *  2. A candidate is «viable» if each argument type is either equal to
 *     the parameter type (an «exact match»), or can be converted to it
 *     (a «conversion»).
 *  3. A viable candidate ‹C› is «better» than another viable candidate
 *     ‹D› if, for every argument, the match for ‹C› is at least as good
 *     as the match for ‹D› (an exact match is better than a
 *     conversion), and for at least one argument, it is strictly
 *     better.
 *  4. If there is a viable candidate which is better than all the
 *     other viable candidates, it is selected and its result type is
 *     the type of the call. Otherwise – no viable candidate, or no
 *     single best one («ambiguity») – the call is an error.
 *
 * Write ‹type_of›, which returns the type of the expression, or
 * ‹std::nullopt› on error. You can assume that all the signatures are
 * well formed and distinct. */

// std::optional< std::string > type_of( const std::vector< std::string > &signatures,
//                                       std::string_view expr );

struct no_match {};

struct candidate { std::vector< std::string > params; std::string result; };

/* 0 = exact match, 1 = conversion, -1 = not viable */
int rank( const std::string &arg, const std::string &param )
{
    if ( arg == param ) return 0;
    if ( arg == "int" && param == "float" ) return 1;
    return -1;
}

std::string check( const std::multimap< std::string, candidate > &funs, const sexp &e )
{
    if ( e.is_atom() )
    {
        if ( e.atom[ 0 ] == '"' ) return "string";
        return e.atom.find( '.' ) == e.atom.npos ? "int" : "float";
    }

    std::vector< std::string > args;
    for ( std::size_t i = 1; i < e.items.size(); ++ i )
        args.push_back( check( funs, e.items[ i ] ) );

    std::vector< std::pair< const candidate *, std::vector< int > > > viable;
    auto [ lo, hi ] = funs.equal_range( e.items[ 0 ].atom );
    for ( auto it = lo; it != hi; ++ it )
    {
        const candidate &c = it->second;
        if ( c.params.size() != args.size() ) continue;
        std::vector< int > ranks;
        for ( std::size_t i = 0; i < args.size(); ++ i )
            ranks.push_back( rank( args[ i ], c.params[ i ] ) );
        if ( std::find( ranks.begin(), ranks.end(), -1 ) == ranks.end() )
            viable.emplace_back( &c, ranks );
    }

    auto better = []( const std::vector< int > &a, const std::vector< int > &b )
    {
        bool strict = false;
        for ( std::size_t i = 0; i < a.size(); ++ i )
        {
            if ( a[ i ] > b[ i ] ) return false;
            strict = strict || a[ i ] < b[ i ];
        }
        return strict;
    };

    for ( const auto &[ c, r ] : viable )
    {
        bool best = true;
        for ( const auto &[ d, s ] : viable )
            if ( d != c && !better( r, s ) )
                best = false;
        if ( best )
            return c->result;
    }

    throw no_match();
}

std::optional< std::string > type_of( const std::vector< std::string > &signatures, std::string_view expr )
{
    std::multimap< std::string, candidate > funs;
    for ( const auto &s : signatures )
    {
        auto space = s.find( ' ' );
        sexp t = read( std::string_view( s ).substr( space + 1 ) );
        candidate c;
        for ( std::size_t i = 1; i + 1 < t.items.size(); ++ i )
            c.params.push_back( t.items[ i ].atom );
        c.result = t.items.back().atom;
        funs.emplace( s.substr( 0, space ), c );
    }

    try { return check( funs, read( expr ) ); }
    catch ( const no_match & ) { return std::nullopt; }
}

int main()
{
    std::vector< std::string > sigs =
    {
        "f (-> int int)",
        "f (-> float float)",
        "g (-> int float string)",
        "g (-> float int string)",
        "h (-> float float float)",
        "k (-> int int string)",
        "k (-> float float int)",
        "len (-> string int)",
        "zero (-> int)",
    };

    auto t = [ & ]( std::string_view e ) { return type_of( sigs, e ); };

    assert( t( "1" ) == "int" );
    assert( t( "2.5" ) == "float" );
    assert( t( "\"hi\"" ) == "string" );
    assert( t( "(f 1)" ) == "int" );
    assert( t( "(f 1.5)" ) == "float" );
    assert( t( "(f (f (f 1)))" ) == "int" );
    assert( t( "(h 1 2)" ) == "float" );
    assert( t( "(g 1 1.0)" ) == "string" );
    assert( t( "(k 1 2)" ) == "string" );
    assert( t( "(k 1.0 2)" ) == "int" );
    assert( t( "(f (len \"abc\"))" ) == "int" );
    assert( t( "(zero)" ) == "int" );
    assert( t( "(h (f 1) (k 1 2.0))" ) == "float" );

    assert( !t( "(g 1 2)" ) );          /* ambiguous */
    assert( !t( "(g 1.0 2.0)" ) );      /* no viable candidate */
    assert( !t( "(f \"x\")" ) );
    assert( !t( "(f 1 2)" ) );
    assert( !t( "(nope 1)" ) );
    assert( !t( "(len 1)" ) );
    assert( !t( "(zero 1)" ) );

    return 0;
}
