#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
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

struct cls
{
    std::string name, parent;
    std::vector< std::pair< std::string, sexp > > fields, methods;
};

std::vector< cls > parse( std::string_view program )
{
    std::vector< cls > out;
    sexp p = read( program );
    for ( std::size_t i = 1; i < p.items.size(); ++ i )
    {
        const auto &c = p.items[ i ].items;
        cls k{ c[ 1 ].atom, c[ 2 ].atom, {}, {} };
        for ( std::size_t j = 1; j < c[ 3 ].items.size(); ++ j )
            k.fields.emplace_back( c[ 3 ].items[ j ].items[ 0 ].atom, c[ 3 ].items[ j ].items[ 1 ] );
        for ( std::size_t j = 1; j < c[ 4 ].items.size(); ++ j )
            k.methods.emplace_back( c[ 4 ].items[ j ].items[ 0 ].atom, c[ 4 ].items[ j ].items[ 1 ] );
        out.push_back( k );
    }
    return out;
}

struct hierarchy
{
    std::map< std::string, const cls * > by_name;

    const cls *parent( const cls &c ) const
    {
        return c.parent == "-" ? nullptr : by_name.at( c.parent );
    }

    bool subtype( const std::string &a, const std::string &b ) const
    {
        if ( a == b ) return true;
        if ( !by_name.contains( a ) ) return false;
        for ( const cls *c = parent( *by_name.at( a ) ); c; c = parent( *c ) )
            if ( c->name == b ) return true;
        return false;
    }

    /* the nearest declaration of a member in ‹c› or its ancestors */
    const sexp *find( const cls *c, const std::string &m, bool method ) const
    {
        for ( ; c; c = parent( *c ) )
            for ( const auto &[ n, t ] : method ? c->methods : c->fields )
                if ( n == m ) return &t;
        return nullptr;
    }
};

void types_in( const sexp &t, std::vector< std::string > &out )
{
    if ( t.is_atom() ) { if ( t.atom != "int" && t.atom != "bool" && t.atom != "->" ) out.push_back( t.atom ); return; }
    for ( const auto &x : t.items ) types_in( x, out );
}

std::optional< std::string > check( std::string_view program )
{
    auto classes = parse( program );
    hierarchy h;

    for ( const auto &c : classes )
        if ( !h.by_name.emplace( c.name, &c ).second )
            return "duplicate class " + c.name;

    for ( const auto &c : classes )
    {
        std::vector< std::string > used;
        if ( c.parent != "-" ) used.push_back( c.parent );
        for ( const auto &[ _, t ] : c.fields ) types_in( t, used );
        for ( const auto &[ _, t ] : c.methods ) types_in( t, used );
        for ( const auto &u : used )
            if ( !h.by_name.contains( u ) )
                return "unknown class " + u;
    }

    for ( const auto &c : classes )
    {
        std::set< std::string > seen;
        for ( const cls *k = &c; k; k = h.parent( *k ) )
            if ( !seen.insert( k->name ).second )
            {
                if ( k == &c ) return "cycle " + c.name;
                break; /* a cycle further up, not through ‹c› */
            }
    }

    for ( const auto &c : classes )
    {
        std::set< std::string > own;
        for ( const auto &[ f, _ ] : c.fields )
            if ( !own.insert( f ).second || h.find( h.parent( c ), f, false ) )
                return "duplicate field " + c.name + "." + f;
    }

    for ( const auto &c : classes )
    {
        std::set< std::string > own;
        for ( const auto &[ m, _ ] : c.methods )
            if ( !own.insert( m ).second )
                return "duplicate method " + c.name + "." + m;
    }

    for ( const auto &c : classes )
        for ( const auto &[ m, t ] : c.methods )
            if ( const sexp *old = h.find( h.parent( c ), m, true ) )
            {
                bool ok = old->items.size() == t.items.size();
                for ( std::size_t i = 1; ok && i + 1 < t.items.size(); ++ i )
                    ok = write( old->items[ i ] ) == write( t.items[ i ] );
                if ( !ok || !h.subtype( t.items.back().atom, old->items.back().atom ) )
                    return "bad override " + c.name + "." + m;
            }

    return std::nullopt;
}

std::optional< std::string > lookup( std::string_view program, std::string_view cls_name,
                                     std::string_view member )
{
    auto classes = parse( program );
    hierarchy h;
    for ( const auto &c : classes )
        h.by_name.emplace( c.name, &c );

    const cls *c = h.by_name.at( std::string( cls_name ) );
    if ( auto t = h.find( c, std::string( member ), false ) ) return write( *t );
    if ( auto t = h.find( c, std::string( member ), true ) ) return write( *t );
    return std::nullopt;
}

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
