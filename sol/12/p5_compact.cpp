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

/* A copying collector needs twice the memory. «Mark & compact»
 * collectors compact the heap «in place»: after marking, the live
 * objects are moved («slid») towards the start of the space, keeping
 * their order. This exercise implements the classic «LISP 2» algorithm,
 * which makes three passes over the heap after marking:
 *
 *  1. compute the «forwarding address» of every marked object: the
 *     total size of the marked objects before it (walking the space
 *     header by header – it is parsable),
 *  2. update every pointer – in the marked objects and in the roots –
 *     to the forwarding address of its target,
 *  3. move every marked object to its forwarding address (in the order
 *     of addresses, so that no object overwrites one which has not
 *     been moved yet).
 *
 * Write ‹compact›, which compacts the space ‹s› in place, updates the
 * roots, and sets ‹s.top› to the end of the last live object. Marking
 * must not be recursive. For the mark bits and the forwarding
 * addresses, you may use vectors indexed by address (real
 * implementations use a bit in the header and an extra header word). */

// void compact( space &s, std::vector< word > &roots );

void compact( space &s, std::vector< word > &roots )
{
    std::vector< bool > marked( s.top );
    std::vector< long > forward( s.top );

    /* mark */
    std::vector< long > stack;
    auto shade = [ & ]( word w )
    {
        if ( is_ptr( w ) && !marked[ untag( w ) ] )
            marked[ untag( w ) ] = true, stack.push_back( untag( w ) );
    };
    for ( auto r : roots ) shade( r );
    while ( !stack.empty() )
    {
        long a = stack.back();
        stack.pop_back();
        for ( long k = 1; k <= s.mem[ a ]; ++ k ) shade( s.mem[ a + k ] );
    }

    /* 1. forwarding addresses */
    long free = 0;
    for ( long a = 0; a < long( s.top ); a += s.mem[ a ] + 1 )
        if ( marked[ a ] )
            forward[ a ] = free, free += s.mem[ a ] + 1;

    /* 2. update pointers */
    auto update = [ & ]( word &w ) { if ( is_ptr( w ) ) w = tag_ptr( forward[ untag( w ) ] ); };
    for ( auto &r : roots ) update( r );
    for ( long a = 0; a < long( s.top ); a += s.mem[ a ] + 1 )
        if ( marked[ a ] )
            for ( long k = 1; k <= s.mem[ a ]; ++ k ) update( s.mem[ a + k ] );

    /* 3. slide */
    for ( long a = 0; a < long( s.top ); )
    {
        long size = s.mem[ a ] + 1;
        if ( marked[ a ] )
            for ( long k = 0; k < size; ++ k ) s.mem[ forward[ a ] + k ] = s.mem[ a + k ];
        a += size;   /* ‹size› was read before the move could overwrite the header */
    }
    s.top = free;
}

int main()
{
    space s( 100 );
    long a = *s.alloc( { tag_int( 1 ) } );
    s.alloc( { tag_int( 9 ), tag_int( 9 ) } );
    long b = *s.alloc( { tag_ptr( a ), tag_int( 2 ) } );
    s.alloc( { tag_ptr( b ) } );
    long c = *s.alloc( { tag_ptr( b ), tag_ptr( a ) } );
    std::vector< word > roots = { tag_ptr( c ), tag_int( 3 ), tag_ptr( a ) };
    std::string before = show( s, roots[ 0 ] );
    assert( before == "(((1) 2) (1))" );

    compact( s, roots );
    assert( s.top == 8 );
    assert(( roots == std::vector< word >{ tag_ptr( 5 ), tag_int( 3 ), tag_ptr( 0 ) } ));
    assert(( std::vector< word >( s.mem.begin(), s.mem.begin() + 8 ) ==
             std::vector< word >{ 1, tag_int( 1 ), 2, tag_ptr( 0 ), tag_int( 2 ), 2, tag_ptr( 2 ), tag_ptr( 0 ) } ));
    assert( show( s, roots[ 0 ] ) == before );

    /* compacting a compact heap changes nothing */
    compact( s, roots );
    assert( s.top == 8 && show( s, roots[ 0 ] ) == before );

    /* a cycle which moves */
    space c2( 20 );
    c2.alloc( { tag_int( 0 ) } );
    long x = *c2.alloc( { tag_int( 1 ), tag_int( 0 ) } );
    c2.mem[ x + 2 ] = tag_ptr( x );
    std::vector< word > cr = { tag_ptr( x ) };
    compact( c2, cr );
    assert( cr[ 0 ] == tag_ptr( 0 ) && c2.mem[ 2 ] == tag_ptr( 0 ) && c2.top == 3 );

    /* many objects, every third one live, in a long list */
    space l( 90000 );
    word list = tag_int( 0 );
    for ( int i = 0; i < 10000; ++ i )
    {
        l.alloc( { tag_int( -1 ) } );
        list = tag_ptr( *l.alloc( { tag_int( i ), list } ) );
        l.alloc( { list, list } );
    }
    std::vector< word > lr = { list };
    compact( l, lr );
    assert( l.top == 30000 );
    word w = lr[ 0 ];
    for ( int i = 9999; i >= 0; -- i )
    {
        assert( untag( w ) == 3 * i );   /* the order is preserved */
        assert( l.mem[ untag( w ) + 1 ] == tag_int( i ) );
        w = l.mem[ untag( w ) + 2 ];
    }

    return 0;
}
