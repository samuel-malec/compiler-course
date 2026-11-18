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

/* Object-oriented languages like Java and C++ use «nominal» subtyping:
 * a class is a subtype of another only if it is declared to be (by
 * inheritance), regardless of its structure. In this exercise, you
 * will check a class hierarchy, given as:
 *
 *     (classes (class NAME PARENT (fields (f T) …) (methods (m T) …)) …)
 *
 * where ‹PARENT› is the name of another class, or ‹-› for a class
 * without a parent; field types are ‹int›, ‹bool› or class names;
 * method types are function types ‹(-> T … R)› over the same types.
 * A class «inherits» all the fields and methods of its parent (and,
 * transitively, its ancestors); it can «override» an inherited method
 * by declaring a method of the same name. Class ‹A› is a subtype of
 * class ‹B› if ‹A› is ‹B› or a descendant of ‹B›; ‹int› and ‹bool› are only
 * subtypes of themselves.
 *
 * Write a function ‹check› which returns the «first» error found by
 * these checks, performed in this order (each over all the classes, in
 * program order), or ‹std::nullopt› if there is none:
 *
 *  1. ‹duplicate class C› – a second class with the same name,
 *  2. ‹unknown class X› – a parent or a type (of a field, or in a method
 *     type) refers to a class which does not exist,
 *  3. ‹cycle C› – the class ‹C› is its own ancestor,
 *  4. ‹duplicate field C.f› – class ‹C› declares a field ‹f› which it
 *     already declares or inherits,
 *  5. ‹duplicate method C.m› – class ‹C› declares a method ‹m› twice,
 *  6. ‹bad override C.m› – an override in class ‹C› has a different number
 *     of parameters than the inherited method, different parameter
 *     types (parameters are «invariant», as in Java), or a result type
 *     which is not a subtype of the inherited one (results are
 *     «covariant»).
 *
 * Also write ‹lookup( program, C, member )›, which (for a hierarchy
 * which passes ‹check›) returns the type of the field or method
 * ‹member› of class ‹C›, as declared in ‹C› or the nearest ancestor
 * which declares it, or ‹std::nullopt› if there is no such member. You
 * can assume that the input is well formed. */

// std::optional< std::string > check( std::string_view program );
// std::optional< std::string > lookup( std::string_view program, std::string_view cls,
//                                      std::string_view member );

int main()
{
    std::string shapes =
        "(classes"
        " (class Shape - (fields (id int)) (methods (area (-> int)) (bigger (-> Shape bool)) (copy (-> Shape))))"
        " (class Circle Shape (fields (r int)) (methods (area (-> int)) (copy (-> Circle))))"
        " (class Dot Circle (fields) (methods (bigger (-> Shape bool)))))";

    assert( !check( shapes ) );
    assert( lookup( shapes, "Dot", "r" ) == "int" );
    assert( lookup( shapes, "Dot", "id" ) == "int" );
    assert( lookup( shapes, "Dot", "copy" ) == "(-> Circle)" );
    assert( lookup( shapes, "Shape", "copy" ) == "(-> Shape)" );
    assert( lookup( shapes, "Circle", "bigger" ) == "(-> Shape bool)" );
    assert( !lookup( shapes, "Shape", "r" ) );

    auto err = []( std::string body ) { return check( "(classes " + body + ")" ); };

    assert( err( "(class A - (fields) (methods)) (class A - (fields) (methods))" ) == "duplicate class A" );
    assert( err( "(class A B (fields) (methods))" ) == "unknown class B" );
    assert( err( "(class A - (fields (x Foo)) (methods))" ) == "unknown class Foo" );
    assert( err( "(class A - (fields) (methods (m (-> Bar int))))" ) == "unknown class Bar" );
    assert( err( "(class A B (fields) (methods)) (class B C (fields) (methods)) (class C A (fields) (methods))" ) ==
            "cycle A" );
    assert( err( "(class A - (fields (x int)) (methods)) (class B A (fields (x bool)) (methods))" ) ==
            "duplicate field B.x" );
    assert( err( "(class A - (fields (x int) (x int)) (methods))" ) == "duplicate field A.x" );
    assert( err( "(class A - (fields) (methods (m (-> int)) (m (-> int))))" ) == "duplicate method A.m" );
    assert( err( "(class A - (fields) (methods (m (-> int int)))) (class B A (fields) (methods (m (-> bool int))))" ) ==
            "bad override B.m" );
    assert( err( "(class A - (fields) (methods (m (-> int int)))) (class B A (fields) (methods (m (-> int int int))))" ) ==
            "bad override B.m" );
    assert( err( "(class A - (fields) (methods (m (-> A)))) (class B A (fields) (methods)) "
                 "(class C B (fields) (methods (m (-> B))))" ) == std::nullopt );
    assert( err( "(class A - (fields) (methods (m (-> A)))) (class B - (fields) (methods)) "
                 "(class C A (fields) (methods (m (-> B))))" ) == "bad override C.m" );
    assert( err( "(class A - (fields) (methods (m (-> A int)))) (class B A (fields) (methods (m (-> B int))))" ) ==
            "bad override B.m" );
    assert( err( "(class A B (fields (x Nope)) (methods)) (class A - (fields) (methods))" ) == "duplicate class A" );
    assert( err( "(class A A (fields (x Nope)) (methods))" ) == "unknown class Nope" );

    return 0;
}
