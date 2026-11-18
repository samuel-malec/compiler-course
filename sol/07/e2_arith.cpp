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

/* Write a type checker for the typed language from the introduction
 * «without» functions and calls: integer and boolean literals,
 * variables, the operators, ‹and›, ‹or›, ‹not›, ‹if› and ‹let›. The
 * function ‹type_of› returns the type of the expression (‹int› or
 * ‹bool›), or ‹std::nullopt› if it is ill-typed or malformed (a
 * malformed expression includes an unknown form and a wrong number of
 * operands). */

// std::optional< std::string > type_of( std::string_view expr );

struct ill_typed {};

std::string check( const sexp &e, const std::map< std::string, std::string > &ctx )
{
    if ( e.is_atom() )
    {
        if ( e.atom == "#t" || e.atom == "#f" ) return "bool";
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
        return h.size() == 1 && h != "<" && h != ">" ? "int" : "bool";
    }
    if ( h == "=" || h == "!=" )
    {
        arity( 2 );
        expect( it[ 2 ], check( it[ 1 ], ctx ) );
        return "bool";
    }
    if ( h == "and" || h == "or" )
    {
        arity( 2 );
        expect( it[ 1 ], "bool" );
        expect( it[ 2 ], "bool" );
        return "bool";
    }
    if ( h == "not" )
    {
        arity( 1 );
        expect( it[ 1 ], "bool" );
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
        if ( !it[ 1 ].is_atom() ) throw ill_typed();
        auto inner = ctx;
        inner[ it[ 1 ].atom ] = check( it[ 2 ], ctx );
        return check( it[ 3 ], inner );
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
    assert( type_of( "1" ) == "int" );
    assert( type_of( "#f" ) == "bool" );
    assert( type_of( "(+ 1 (* 2 3))" ) == "int" );
    assert( type_of( "(< 1 2)" ) == "bool" );
    assert( type_of( "(= #t (not #f))" ) == "bool" );
    assert( type_of( "(if (and #t (<= 1 2)) 3 4)" ) == "int" );
    assert( type_of( "(let x 5 (let y (> x 1) (if y x 0)))" ) == "int" );
    assert( type_of( "(let x #t (let x 1 (+ x 1)))" ) == "int" );

    assert( !type_of( "(+ 1 #t)" ) );
    assert( !type_of( "(= 1 #t)" ) );
    assert( !type_of( "(if 1 2 3)" ) );
    assert( !type_of( "(if #t 1 #f)" ) );
    assert( !type_of( "(not 1)" ) );
    assert( !type_of( "(or (< 1 2) 3)" ) );
    assert( !type_of( "x" ) );
    assert( !type_of( "(+ 1)" ) );
    assert( !type_of( "(xor #t #f)" ) );

    return 0;
}
