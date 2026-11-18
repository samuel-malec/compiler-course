#include <cassert>
#include <optional>
#include <string>
#include <vector>

/* Flat memory (see ‹d2_copying›). A ‹space› is a vector of words with
 * an allocation pointer ‹top›: the words below ‹top› are in use, and
 * allocation simply moves ‹top› up («bump allocation»). An object at
 * address ‹a› is a «header» word ‹mem[ a ]›, which holds the number of
 * fields ‹n›, followed by the fields ‹mem[ a + 1 ]› … ‹mem[ a + n ]›.
 *
 * Every field is a «tagged» word: an odd word ‹2x + 1› is the integer
 * ‹x›, an even word ‹2a› is a pointer to the object at address ‹a› (this
 * is how OCaml and many implementations of Lisp tell pointers from
 * integers). A collector can thus find all the pointers in an object
 * without knowing anything about its type, and the space is
 * «parsable»: walking from address 0, header by header, visits every
 * object.
 *
 * ‹alloc› returns the address of a new object, or ‹std::nullopt› if
 * there is not enough room. ‹show› prints the structure reachable from a
 * word as nested lists (e.g. ‹(1 (2 3))›), down to the given depth (‹…›
 * below it), with ‹?› for a pointer to something which is not an
 * object. */

using word = long;

word tag_int( long x ) { return 2 * x + 1; }
word tag_ptr( long a ) { return 2 * a; }
bool is_ptr( word w ) { return ( w & 1 ) == 0; }
long untag( word w ) { return w >> 1; }

struct space
{
    std::vector< word > mem;
    std::size_t top = 0;

    explicit space( std::size_t size ) : mem( size ) {}

    std::optional< long > alloc( const std::vector< word > &fields )
    {
        if ( top + 1 + fields.size() > mem.size() ) return std::nullopt;
        long a = top;
        mem[ top++ ] = fields.size();
        for ( auto w : fields ) mem[ top++ ] = w;
        return a;
    }
};

std::string show( const space &s, word w, int depth = 20 )
{
    if ( !is_ptr( w ) ) return std::to_string( untag( w ) );
    long a = untag( w );
    if ( a < 0 || a >= long( s.top ) || s.mem[ a ] < 0 || a + s.mem[ a ] >= long( s.top ) ) return "?";
    if ( depth == 0 ) return "…";
    std::string out = "(";
    for ( long k = 0; k < s.mem[ a ]; ++ k )
        out += ( k ? " " : "" ) + show( s, s.mem[ a + 1 + k ], depth - 1 );
    return out + ")";
}

/* The copying collector of ‹d2_copying› is recursive. Cheney's
 * algorithm (1970) copies «breadth-first» and needs neither recursion
 * nor a stack: to-space itself serves as the queue. It keeps two
 * pointers into to-space – ‹scan› and ‹top›. Objects below ‹scan› have
 * been copied «and» their fields updated («black»); objects between
 * ‹scan› and ‹top› have been copied, but their fields still point into
 * from-space («grey»).
 *
 *  1. copy the objects referenced by the roots (in the order of the
 *     roots) and update the roots,
 *  2. while ‹scan < top›: for each field of the object at ‹scan›
 *     (in order), if it points to from-space, copy the object (unless
 *     it has already been copied) and update the field to the address
 *     of the copy; then move ‹scan› past the object.
 *
 * Copying an object only reserves space at ‹top› and copies the header
 * and fields unchanged; the original's header becomes a forwarding
 * pointer (‹-1 - new address›, as in ‹d2_copying›). Write ‹cheney›, which
 * collects ‹from› into a new space of the same size and returns it. */

// space cheney( space &from, std::vector< word > &roots );

space cheney( space &from, std::vector< word > &roots )
{
    space to( from.mem.size() );

    auto copy = [ & ]( word w ) -> word
    {
        if ( !is_ptr( w ) ) return w;
        long a = untag( w );
        if ( from.mem[ a ] < 0 ) return tag_ptr( -1 - from.mem[ a ] );
        long n = from.mem[ a ], b = to.top;
        for ( long k = 0; k <= n; ++ k ) to.mem[ b + k ] = from.mem[ a + k ];
        to.top += n + 1;
        from.mem[ a ] = -1 - b;
        return tag_ptr( b );
    };

    for ( auto &r : roots ) r = copy( r );

    for ( std::size_t scan = 0; scan < to.top; )
    {
        long n = to.mem[ scan ];
        for ( long k = 1; k <= n; ++ k )
            to.mem[ scan + k ] = copy( to.mem[ scan + k ] );
        scan += n + 1;
    }
    return to;
}

int main()
{
    /* a tree: 1 → (2, 3), 2 → (4), 3 → (4, 5) */
    space s( 100 );
    long n5 = *s.alloc( { tag_int( 5 ) } );
    long n4 = *s.alloc( { tag_int( 4 ) } );
    s.alloc( { tag_int( 0 ), tag_int( 0 ), tag_int( 0 ) } );   /* garbage */
    long n3 = *s.alloc( { tag_ptr( n4 ), tag_ptr( n5 ) } );
    long n2 = *s.alloc( { tag_ptr( n4 ) } );
    long n1 = *s.alloc( { tag_ptr( n2 ), tag_ptr( n3 ) } );
    std::string before = show( s, tag_ptr( n1 ) );
    assert( before == "(((4)) ((4) (5)))" );

    std::vector< word > roots = { tag_ptr( n1 ) };
    space t = cheney( s, roots );
    assert( show( t, roots[ 0 ] ) == before );
    assert( t.top == 12 );

    /* breadth-first order: 1 at 0, 2 at 3, 3 at 5, 4 at 8, 5 at 10 */
    assert( roots[ 0 ] == tag_ptr( 0 ) );
    assert(( std::vector< word >( t.mem.begin(), t.mem.begin() + 12 ) ==
             std::vector< word >{ 2, tag_ptr( 3 ), tag_ptr( 5 ),
                                  1, tag_ptr( 8 ),
                                  2, tag_ptr( 8 ), tag_ptr( 10 ),
                                  1, tag_int( 4 ),
                                  1, tag_int( 5 ) } ));

    /* roots are copied first, in order; integer roots stay */
    std::vector< word > r2 = { tag_int( 7 ), roots[ 0 ], tag_ptr( 10 ) };
    space u = cheney( t, r2 );
    assert(( r2 == std::vector< word >{ tag_int( 7 ), tag_ptr( 0 ), tag_ptr( 3 ) } ));
    assert( show( u, r2[ 1 ] ) == before );

    /* a cycle, an empty object */
    space c( 30 );
    long e = *c.alloc( {} );
    long x = *c.alloc( { tag_int( 1 ), tag_ptr( e ) } );
    long y = *c.alloc( { tag_ptr( x ) } );
    c.mem[ x + 1 ] = tag_ptr( y );
    std::vector< word > cr = { tag_ptr( x ) };
    space d = cheney( c, cr );
    assert( d.top == 6 );
    assert( show( d, cr[ 0 ], 4 ) == "((((…) ())) ())" );

    /* a long list: no recursion */
    space l( 300000 );
    word list = tag_int( 0 );
    for ( int i = 0; i < 50000; ++ i ) list = tag_ptr( *l.alloc( { tag_int( i ), list } ) ), l.alloc( { list } );
    std::vector< word > lr = { list };
    space m = cheney( l, lr );
    assert( m.top == 150000 );
    word w = lr[ 0 ];
    for ( int i = 49999; i >= 0; -- i )
    {
        assert( m.mem[ untag( w ) + 1 ] == tag_int( i ) );
        w = m.mem[ untag( w ) + 2 ];
    }
    assert( w == tag_int( 0 ) );

    return 0;
}
