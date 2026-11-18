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

/* Subtyping and mutation do not mix easily. Extend the language of
 * ‹d2_subtype› (types ‹int›, ‹bool›, ‹top›, records, functions; ‹rec›,
 * ‹get›, ‹if› with joins, ‹fn› and calls with subsumption) with all the
 * remaining forms of the typed language from the introduction (the
 * operators, ‹and›, ‹or›, ‹not› and ‹let›), and with «references»:
 *
 *  • the type ‹(ref T)› of a mutable cell holding a ‹T›,
 *  • ‹(new e)› – a new cell, initialized to ‹e›; type ‹(ref T)› where ‹T›
 *    is the type of ‹e›,
 *  • ‹(deref r)› – the content of the cell ‹r› (of type ‹T› if ‹r› is a
 *    ‹(ref T)›),
 *  • ‹(assign r e)› – store ‹e› into the cell ‹r›, which must have type
 *    ‹(ref T)›; the type of ‹e› must be a subtype of ‹T›; the type of the
 *    expression is ‹T›.
 *
 * The subtyping rule for references is the interesting part: ‹(ref
 * S) <: (ref T)› holds only if ‹S› and ‹T› are «equivalent» (each is a
 * subtype of the other). References are «invariant»: covariance
 * would let a function which expects a ‹(ref (record (x int)))› store
 * a record «without» ‹y› into a cell which its caller believes holds
 * records «with» ‹y›; contravariance would let it read a record
 * without ‹y› from a cell declared to hold them.
 *
 * Use the subtyping and join rules from ‹p3_subtype›; the join of two
 * reference types is the first one if they are equivalent, and ‹top›
 * otherwise. Write ‹type_of›, which returns the type of an expression
 * in the canonical form of ‹p3_subtype›, or ‹std::nullopt› if it is
 * ill-typed. The comparison operators work as in the introduction
 * (‹=› and ‹!=› only on ‹int› and ‹bool›). */

// std::optional< std::string > type_of( std::string_view expr );

int main()
{
    std::string xy = "(record (x int) (y int))", x = "(record (x int))";

    assert( type_of( "(new 1)" ) == "(ref int)" );
    assert( type_of( "(deref (new #t))" ) == "bool" );
    assert( type_of( "(let r (new 1) (assign r (+ (deref r) 1)))" ) == "int" );
    assert( type_of( "(new (rec (y 2) (x 1)))" ) == "(ref " + xy + ")" );

    /* storing a subtype into a cell is fine */
    assert( type_of( "(let r (new (rec (x 1))) (assign r (rec (x 2) (y 3))))" ) == x );

    /* but a reference to a subtype is not a reference to the supertype */
    std::string f = "(fn ((r (ref " + x + "))) (assign r (rec (x 0))))";
    assert( type_of( "(" + f + " (new (rec (x 1))))" ) == x );
    assert( !type_of( "(" + f + " (new (rec (x 1) (y 2))))" ) );

    /* reading through a reference and then subsuming is fine */
    std::string g = "(fn ((p " + x + ")) (get p x))";
    assert( type_of( "(" + g + " (deref (new (rec (x 1) (y 2)))))" ) == "int" );

    /* joins */
    assert( type_of( "(if #t (new 1) (new 2))" ) == "(ref int)" );
    assert( type_of( "(if #t (new (rec (x 1))) (new (rec (x 1) (y 2))))" ) == "top" );
    assert( type_of( "(fn ((r (ref (record (y int) (x int))))) r)" ) == "(-> (ref " + xy + ") (ref " + xy + "))" );
    assert( type_of( "((fn ((r (ref (record (y int) (x int))))) (deref r)) (new (rec (x 1) (y 2))))" ) == xy );

    assert( !type_of( "(deref 1)" ) );
    assert( !type_of( "(assign (new 1) #t)" ) );
    assert( !type_of( "(let r (new (rec (x 1) (y 2))) (assign r (rec (x 2))))" ) );
    assert( !type_of( "(= (new 1) (new 1))" ) );

    return 0;
}
