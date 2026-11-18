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

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
bool is( const type &t, std::string_view h ) { return t.is_list() && !t.items.empty() && t.items[ 0 ].atom == h; }

std::map< std::string, type > fields( const type &r )
{
    std::map< std::string, type > out;
    for ( std::size_t i = 1; i < r.items.size(); ++ i )
        out.emplace( r.items[ i ].items[ 0 ].atom, r.items[ i ].items[ 1 ] );
    return out;
}

bool subtype( const type &s, const type &t )
{
    if ( t.is_atom() && t.atom == "top" ) return true;
    if ( s.is_atom() || t.is_atom() ) return s.is_atom() && t.is_atom() && s.atom == t.atom;

    if ( is( s, "record" ) && is( t, "record" ) )
    {
        auto fs = fields( s );
        for ( const auto &[ n, ft ] : fields( t ) )
            if ( !fs.contains( n ) || !subtype( fs.at( n ), ft ) )
                return false;
        return true;
    }

    if ( is( s, "ref" ) && is( t, "ref" ) ) /* invariant */
        return subtype( s.items[ 1 ], t.items[ 1 ] ) && subtype( t.items[ 1 ], s.items[ 1 ] );

    if ( is( s, "->" ) && is( t, "->" ) && s.items.size() == t.items.size() )
    {
        for ( std::size_t i = 1; i + 1 < s.items.size(); ++ i )
            if ( !subtype( t.items[ i ], s.items[ i ] ) )
                return false;
        return subtype( s.items.back(), t.items.back() );
    }

    return false;
}

/* Sort the fields of all records (recursively). */
type canonical( const type &t )
{
    if ( t.is_atom() ) return t;
    type out{ "", { t.items[ 0 ] }, 0 };
    if ( is( t, "record" ) )
        for ( const auto &[ n, ft ] : fields( t ) )
            out.items.push_back( sexp{ "", { atom( n ), canonical( ft ) }, 0 } );
    else
        for ( std::size_t i = 1; i < t.items.size(); ++ i )
            out.items.push_back( canonical( t.items[ i ] ) );
    return out;
}

type join( const type &a, const type &b )
{
    if ( subtype( a, b ) ) return canonical( b );
    if ( subtype( b, a ) ) return canonical( a );

    if ( is( a, "record" ) && is( b, "record" ) )
    {
        type out{ "", { atom( "record" ) }, 0 };
        auto fb = fields( b );
        for ( const auto &[ n, ft ] : fields( a ) )
            if ( fb.contains( n ) )
                out.items.push_back( sexp{ "", { atom( n ), join( ft, fb.at( n ) ) }, 0 } );
        return out;
    }

    if ( is( a, "->" ) && is( b, "->" ) && a.items.size() == b.items.size() )
    {
        type out{ "", { atom( "->" ) }, 0 };
        for ( std::size_t i = 1; i + 1 < a.items.size(); ++ i )
        {
            if ( !subtype( a.items[ i ], b.items[ i ] ) || !subtype( b.items[ i ], a.items[ i ] ) )
                return atom( "top" );
            out.items.push_back( canonical( a.items[ i ] ) );
        }
        out.items.push_back( join( a.items.back(), b.items.back() ) );
        return out;
    }

    return atom( "top" );
}


struct ill_typed {};

bool valid( const type &t )
{
    if ( t.is_atom() ) return t.atom == "int" || t.atom == "bool" || t.atom == "top";
    if ( t.items.empty() ) return false;
    const std::string &h = t.items[ 0 ].atom;
    if ( h == "ref" ) return t.items.size() == 2 && valid( t.items[ 1 ] );
    if ( h == "->" )
    {
        if ( t.items.size() < 2 ) return false;
        for ( std::size_t i = 1; i < t.items.size(); ++ i ) if ( !valid( t.items[ i ] ) ) return false;
        return true;
    }
    if ( h == "record" )
    {
        std::map< std::string, int > seen;
        for ( std::size_t i = 1; i < t.items.size(); ++ i )
        {
            const auto &f = t.items[ i ];
            if ( !f.is_list() || f.items.size() != 2 || !f.items[ 0 ].is_atom() ||
                 seen[ f.items[ 0 ].atom ]++ || !valid( f.items[ 1 ] ) )
                return false;
        }
        return true;
    }
    return false;
}

using context = std::map< std::string, type >;

type check( const sexp &e, const context &ctx )
{
    const type int_t = atom( "int" ), bool_t = atom( "bool" );
    auto same = []( const type &a, const type &b ) { return subtype( a, b ) && subtype( b, a ); };

    if ( e.is_atom() )
    {
        if ( e.atom == "#t" || e.atom == "#f" ) return bool_t;
        if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos ) return int_t;
        if ( auto it = ctx.find( e.atom ); it != ctx.end() ) return it->second;
        throw ill_typed();
    }

    const auto &it = e.items;
    if ( it.empty() ) throw ill_typed();
    const std::string &h = it[ 0 ].atom;
    auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw ill_typed(); };
    auto expect = [ & ]( const sexp &x, const type &t ) { if ( !same( check( x, ctx ), t ) ) throw ill_typed(); };

    if ( h == "+" || h == "-" || h == "*" || h == "/" || h == "<" || h == "<=" || h == ">" || h == ">=" )
    {
        arity( 2 );
        expect( it[ 1 ], int_t );
        expect( it[ 2 ], int_t );
        return ( h == "+" || h == "-" || h == "*" || h == "/" ) ? int_t : bool_t;
    }
    if ( h == "=" || h == "!=" )
    {
        arity( 2 );
        type t = check( it[ 1 ], ctx );
        if ( !same( t, int_t ) && !same( t, bool_t ) ) throw ill_typed();
        expect( it[ 2 ], t );
        return bool_t;
    }
    if ( h == "and" || h == "or" || h == "not" )
    {
        arity( h == "not" ? 1 : 2 );
        for ( std::size_t i = 1; i < it.size(); ++ i ) expect( it[ i ], bool_t );
        return bool_t;
    }
    if ( h == "if" )
    {
        arity( 3 );
        expect( it[ 1 ], bool_t );
        return join( check( it[ 2 ], ctx ), check( it[ 3 ], ctx ) );
    }
    if ( h == "let" )
    {
        arity( 3 );
        context inner = ctx;
        inner.insert_or_assign( it[ 1 ].atom, check( it[ 2 ], ctx ) );
        return check( it[ 3 ], inner );
    }
    if ( h == "rec" )
    {
        type out{ "", { atom( "record" ) }, 0 };
        std::map< std::string, int > seen;
        for ( std::size_t i = 1; i < it.size(); ++ i )
        {
            if ( !it[ i ].is_list() || it[ i ].items.size() != 2 || seen[ it[ i ].items[ 0 ].atom ]++ )
                throw ill_typed();
            out.items.push_back( sexp{ "", { it[ i ].items[ 0 ], check( it[ i ].items[ 1 ], ctx ) }, 0 } );
        }
        return canonical( out );
    }
    if ( h == "get" )
    {
        arity( 2 );
        type r = check( it[ 1 ], ctx );
        if ( !is( r, "record" ) || !fields( r ).contains( it[ 2 ].atom ) ) throw ill_typed();
        return fields( r ).at( it[ 2 ].atom );
    }
    if ( h == "new" )
    {
        arity( 1 );
        return sexp{ "", { atom( "ref" ), check( it[ 1 ], ctx ) }, 0 };
    }
    if ( h == "deref" )
    {
        arity( 1 );
        type r = check( it[ 1 ], ctx );
        if ( !is( r, "ref" ) ) throw ill_typed();
        return r.items[ 1 ];
    }
    if ( h == "assign" )
    {
        arity( 2 );
        type r = check( it[ 1 ], ctx );
        if ( !is( r, "ref" ) || !subtype( check( it[ 2 ], ctx ), r.items[ 1 ] ) ) throw ill_typed();
        return r.items[ 1 ];
    }
    if ( h == "fn" )
    {
        arity( 2 );
        context inner = ctx;
        type out{ "", { atom( "->" ) }, 0 };
        for ( const auto &p : it[ 1 ].items )
        {
            if ( !p.is_list() || p.items.size() != 2 || !valid( p.items[ 1 ] ) ) throw ill_typed();
            type pt = canonical( p.items[ 1 ] );
            inner.insert_or_assign( p.items[ 0 ].atom, pt );
            out.items.push_back( pt );
        }
        out.items.push_back( check( it[ 2 ], inner ) );
        return out;
    }

    type f = check( it[ 0 ], ctx );
    if ( !is( f, "->" ) || f.items.size() != it.size() + 1 ) throw ill_typed();
    for ( std::size_t i = 1; i < it.size(); ++ i )
        if ( !subtype( check( it[ i ], ctx ), f.items[ i ] ) ) throw ill_typed();
    return f.items.back();
}

std::optional< std::string > type_of( std::string_view expr )
{
    try { return write( canonical( check( read( expr ), {} ) ) ); }
    catch ( const ill_typed & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

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
