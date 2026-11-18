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

/* As explained in the introduction, the meaning of a variable has two
 * parts: the «environment» maps the name to a «location», and the
 * «store» maps the location to a value. In this exercise, the two
 * must be kept separate, because several names can refer to the same
 * location. Implement the core language (without ‹if› and ‹while›)
 * extended as follows:
 *
 *  • ‹(var x e)› allocates a new location, initialized to the value of
 *    ‹e›, and binds ‹x› to it,
 *  • ‹(array a n)›, where ‹n› is a positive integer literal, allocates
 *    ‹n› new consecutive locations, initialized to 0, and binds the
 *    «array» ‹a› to them,
 *  • ‹(ref r x)› binds ‹r› to the «same» location as the (scalar)
 *    variable ‹x›; ‹(ref r a i)› binds ‹r› to the location of element
 *    ‹i› of the array ‹a›, where ‹i› is an expression evaluated when
 *    the ‹ref› is executed; neither allocates anything,
 *  • ‹(at a i)› is an expression: the value of element ‹i› of ‹a›,
 *    ‹(store a i e)› is a statement: set element ‹i› of ‹a› to ‹e›,
 *  • ‹(set x e)› stores into the location of ‹x› (which may be a ‹ref›).
 *
 * Locations allocated in a block are released at the end of that
 * block (they form a stack, like the C++ call stack). The result of
 * ‹run› is the printed values together with the «peak» number of
 * locations allocated at the same time. Return ‹std::nullopt› for the
 * errors of ‹e1_print›, for an array index out of range, and for using
 * an array where a scalar is expected or vice versa (all names –
 * ‹var›, ‹array› and ‹ref› – share one namespace per block). */

struct result
{
    std::vector< long > output;
    std::size_t peak;
};

// std::optional< result > run( std::string_view program );

struct bad {};

/* What a name is bound to: a location, and the number of locations
 * (0 for a scalar, the length for an array). */

struct binding { std::size_t loc; std::size_t length; };

struct machine
{
    std::vector< std::map< std::string, binding > > env{ {} };
    std::vector< long > store = {};
    std::vector< long > out = {};
    std::size_t peak = 0;

    const binding &find( const std::string &n )
    {
        for ( auto it = env.rbegin(); it != env.rend(); ++ it )
            if ( auto b = it->find( n ); b != it->end() )
                return b->second;
        throw bad();
    }

    std::size_t scalar( const sexp &name )
    {
        if ( !name.is_atom() ) throw bad();
        auto b = find( name.atom );
        if ( b.length ) throw bad();
        return b.loc;
    }

    std::size_t element( const sexp &name, const sexp &index )
    {
        if ( !name.is_atom() ) throw bad();
        auto b = find( name.atom );
        long i = eval( index );
        if ( !b.length || i < 0 || i >= long( b.length ) ) throw bad();
        return b.loc + i;
    }

    std::size_t allocate( std::size_t n, long init )
    {
        std::size_t loc = store.size();
        store.resize( loc + n, init );
        peak = std::max( peak, store.size() );
        return loc;
    }

    void bind( const sexp &name, binding b )
    {
        if ( !name.is_atom() || !env.back().emplace( name.atom, b ).second )
            throw bad();
    }

    long eval( const sexp &e )
    {
        if ( e.is_atom() )
        {
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos && e.atom != "-" )
                return std::stol( e.atom );
            return store[ scalar( e ) ];
        }
        if ( e.items.size() != 3 || !e.items[ 0 ].is_atom() )
            throw bad();
        const std::string &op = e.items[ 0 ].atom;
        if ( op == "at" )
            return store[ element( e.items[ 1 ], e.items[ 2 ] ) ];
        long a = eval( e.items[ 1 ] ), b = eval( e.items[ 2 ] );
        if ( op == "+" ) return a + b;
        if ( op == "-" ) return a - b;
        if ( op == "*" ) return a * b;
        if ( op == "<" ) return a < b;
        if ( op == "<=" ) return a <= b;
        if ( op == ">" ) return a > b;
        if ( op == ">=" ) return a >= b;
        if ( op == "=" ) return a == b;
        if ( op == "!=" ) return a != b;
        if ( ( op == "/" || op == "%" ) && b != 0 ) return op == "/" ? a / b : a % b;
        throw bad();
    }

    void exec( const sexp &s )
    {
        if ( s.is_atom() || s.items.empty() ) throw bad();
        const std::string &head = s.items[ 0 ].atom;
        auto n = s.items.size();

        if ( head == "block" )
        {
            env.emplace_back();
            std::size_t mark = store.size();
            for ( std::size_t i = 1; i < n; ++ i )
                exec( s.items[ i ] );
            store.resize( mark ); /* release the block's locations */
            env.pop_back();
        }
        else if ( head == "var" && n == 3 )
        {
            long v = eval( s.items[ 2 ] );
            bind( s.items[ 1 ], { allocate( 1, v ), 0 } );
        }
        else if ( head == "array" && n == 3 && s.items[ 2 ].is_atom() &&
                  s.items[ 2 ].atom.find_first_not_of( "0123456789" ) == std::string::npos )
        {
            long len = std::stol( s.items[ 2 ].atom );
            if ( len <= 0 ) throw bad();
            bind( s.items[ 1 ], { allocate( len, 0 ), std::size_t( len ) } );
        }
        else if ( head == "ref" && n == 3 )
            bind( s.items[ 1 ], { scalar( s.items[ 2 ] ), 0 } );
        else if ( head == "ref" && n == 4 )
            bind( s.items[ 1 ], { element( s.items[ 2 ], s.items[ 3 ] ), 0 } );
        else if ( head == "set" && n == 3 )
        {
            long v = eval( s.items[ 2 ] );
            store[ scalar( s.items[ 1 ] ) ] = v;
        }
        else if ( head == "store" && n == 4 )
        {
            std::size_t loc = element( s.items[ 1 ], s.items[ 2 ] );
            store[ loc ] = eval( s.items[ 3 ] );
        }
        else if ( head == "print" && n == 2 )
            out.push_back( eval( s.items[ 1 ] ) );
        else
            throw bad();
    }
};

std::optional< result > run( std::string_view program )
{
    try
    {
        machine m;
        m.exec( read( program ) );
        return result{ m.out, m.peak };
    }
    catch ( const bad & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    using v = std::vector< long >;

    auto r = run( "(block (var x 1) (ref y x) (set y 5) (print x) (set x 7) (print y))" );
    assert( r && ( r->output == v{ 5, 7 } ) && r->peak == 1 );

    r = run( "(block (array a 3) (store a 1 10) (ref e a 1) (set e (+ e 1))"
             " (print (at a 1)) (print (at a 0)))" );
    assert( r && ( r->output == v{ 11, 0 } ) && r->peak == 3 );

    r = run( "(block (var i 0) (array a 2) (ref p a i) (set i 1) (set p 9)"
             " (print (at a 0)) (print (at a 1)))" );
    assert( r && ( r->output == v{ 9, 0 } ) ); /* the index is fixed when ‹ref› runs */

    r = run( "(block (var a 1) (block (var b 2) (var c 3)) (block (array d 4)) (var e 5) (print a))" );
    assert( r && r->peak == 5 && ( r->output == v{ 1 } ) );

    r = run( "(block (var x 1) (block (ref x x) (set x 2)) (print x))" );
    assert( r && ( r->output == v{ 2 } ) && r->peak == 1 );

    assert( !run( "(block (array a 2) (print (at a 2)))" ) );
    assert( !run( "(block (array a 2) (ref r a -1))" ) );
    assert( !run( "(block (array a 2) (print a))" ) );
    assert( !run( "(block (var x 1) (print (at x 0)))" ) );
    assert( !run( "(block (array a 2) (ref r a))" ) );
    assert( !run( "(block (array a 0))" ) );
    assert( !run( "(block (var x 1) (ref x x))" ) );

    return 0;
}
