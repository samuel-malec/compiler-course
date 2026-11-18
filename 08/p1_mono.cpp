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

/* Write a type inference function for the language from the
 * introduction, «without» let-polymorphism: ‹(let x e b)› is treated
 * like ‹((fn (x) b) e)› – the variable ‹x› gets the type of ‹e›, and
 * that type is the same at all uses of ‹x›. Every other construct
 * behaves as in ‹d2_infer›. The function ‹infer› returns the principal
 * type of the expression, in normal form (see the introduction), or
 * ‹std::nullopt› if the expression has no type or is malformed. */

// std::optional< std::string > infer( std::string_view expr );

int main()
{
    assert( infer( "1" ) == "int" );
    assert( infer( "(fn (x) x)" ) == "(-> 'a 'a)" );
    assert( infer( "(fn (x y) x)" ) == "(-> 'a 'b 'a)" );
    assert( infer( "(fn (x) (if x 1 2))" ) == "(-> bool int)" );
    assert( infer( "(fn (f) (f 1))" ) == "(-> (-> int 'a) 'a)" );
    assert( infer( "(fn (f g x) (g (f x)))" ) == "(-> (-> 'a 'b) (-> 'b 'c) 'a 'c)" );
    assert( infer( "(fn (x y) (and (= x y) (< y 2)))" ) == "(-> int int bool)" );
    assert( infer( "(fn (x y) (= x y))" ) == "(-> 'a 'a bool)" );
    assert( infer( "(let id (fn (x) x) (id 1))" ) == "int" );
    assert( infer( "(fn (k) (fn (x) (fn (y) ((k y) x))))" ) == "(-> (-> 'a (-> 'b 'c)) (-> 'b (-> 'a 'c)))" );
    assert( infer( "(fn () 5)" ) == "(-> int)" );
    assert( infer( "((fn (f) (f)) (fn () #t))" ) == "bool" );

    assert( !infer( "(let id (fn (x) x) (if (id #t) (id 1) 0))" ) ); /* no polymorphism */
    assert( !infer( "(fn (x) (x x))" ) );
    assert( !infer( "(+ 1 #f)" ) );
    assert( !infer( "((fn (x) x) 1 2)" ) );
    assert( !infer( "(= (fn (x) x) (fn (x) x))" ) );
    assert( !infer( "(fn (f) (= f (fn (x) 1)))" ) );
    assert( !infer( "z" ) );
    assert( !infer( "(if 1)" ) );

    return 0;
}
