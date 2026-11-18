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

/* A type inference engine does not only decide whether a program has
 * a type – it can also «tell» the rest of the compiler the type of
 * every expression. Translating an implicitly typed program into an
 * explicitly typed one is called «elaboration». Write a function
 * ‹elaborate› which takes a program in the language of ‹p1_mono›
 * (monomorphic ‹let›), infers its type, and returns the program with
 * every ‹fn› parameter annotated with its inferred type, in the
 * syntax of Chapter 7: ‹(fn (x y) e)› becomes ‹(fn ((x T) (y U)) e)›.
 *
 * The types are those of the «final» substitution (after the whole
 * program has been inferred). Type variables which remain are
 * normalized «together», for the whole output: they are renamed to
 * ‹'a›, ‹'b›, … in the order of their first appearance in the written
 * output. Everything else in the program is unchanged. Return
 * ‹std::nullopt› if the program has no type. */

// std::optional< std::string > elaborate( std::string_view program );

int main()
{
    assert( elaborate( "1" ) == "1" );
    assert( elaborate( "(fn (x) x)" ) == "(fn ((x 'a)) x)" );
    assert( elaborate( "(fn (x) (+ x 1))" ) == "(fn ((x int)) (+ x 1))" );
    assert( elaborate( "(fn (f x) (f x))" ) == "(fn ((f (-> 'a 'b)) (x 'a)) (f x))" );
    assert( elaborate( "(let id (fn (x) x) (id 1))" ) == "(let id (fn ((x int)) x) (id 1))" );
    assert( elaborate( "(fn (g) (fn (y) (if (g y) y 0)))" ) == "(fn ((g (-> int bool))) (fn ((y int)) (if (g y) y 0)))" );
    assert( elaborate( "(fn (a b) ((fn (c) a) b))" ) == "(fn ((a 'a) (b 'b)) ((fn ((c 'b)) a) b))" );
    assert( elaborate( "(fn () 1)" ) == "(fn () 1)" );

    assert( !elaborate( "(fn (x) (x x))" ) );
    assert( !elaborate( "(let id (fn (x) x) (if (id #t) (id 1) 0))" ) );

    return 0;
}
