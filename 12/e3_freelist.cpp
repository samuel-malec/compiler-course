#include <cassert>
#include <optional>
#include <stdexcept>
#include <vector>

/* Implement a «free list» allocator, which manages the addresses
 * ‹0 … size - 1› of some memory (it does not store anything in the
 * memory itself). The free memory is a list of «free blocks» (a start
 * address and a length), kept sorted by address.
 *
 *  • ‹alloc( n )› uses «first fit»: it finds the free block with the
 *    lowest address which has at least ‹n› words, takes the first ‹n›
 *    words of it (the rest of the block stays free) and returns their
 *    address; if there is no such block, or ‹n› is 0, it returns
 *    ‹std::nullopt›,
 *  • ‹release( addr )› frees the block allocated at ‹addr› (the
 *    allocator remembers the sizes) and «coalesces» it with the
 *    neighbouring free blocks, so that no two free blocks are ever
 *    adjacent; releasing an address which is not the start of an
 *    allocated block throws ‹std::invalid_argument›,
 *  • ‹free_words()› is the total size of the free blocks, ‹largest()›
 *    the size of the largest one (0 if there are none), and ‹blocks()›
 *    their number.
 *
 * Notice in the tests how «fragmentation» arises: there is enough free
 * memory in total, but no free block is large enough. */

// class freelist;

int main()
{
    freelist f( 100 );
    assert( f.free_words() == 100 && f.largest() == 100 && f.blocks() == 1 );

    auto a = f.alloc( 10 ), b = f.alloc( 20 ), c = f.alloc( 30 );
    assert( a == 0 && b == 10 && c == 30 );
    assert( f.free_words() == 40 && f.blocks() == 1 );
    assert( !f.alloc( 41 ) );
    assert( !f.alloc( 0 ) );

    f.release( *b );
    assert( f.free_words() == 60 && f.blocks() == 2 && f.largest() == 40 );
    assert( f.alloc( 5 ) == 10 );        /* first fit: the hole left by ‹b› */
    assert( f.alloc( 20 ) == 60 );       /* 15 words left in the hole: too small */
    assert( f.alloc( 15 ) == 15 );       /* fills the hole exactly */
    assert( f.blocks() == 1 && f.largest() == 20 );

    /* fragmentation: 4 × 20 words free, but not 30 in one piece */
    freelist g( 160 );
    std::vector< std::size_t > xs;
    for ( int i = 0; i < 8; ++ i ) xs.push_back( *g.alloc( 20 ) );
    for ( int i = 0; i < 8; i += 2 ) g.release( xs[ i ] );
    assert( g.free_words() == 80 && g.largest() == 20 && g.blocks() == 4 );
    assert( !g.alloc( 30 ) );

    /* coalescing on both sides */
    g.release( xs[ 1 ] );
    assert( g.blocks() == 3 && g.largest() == 60 );
    assert( g.alloc( 30 ) == 0 );

    bool caught = false;
    try { g.release( 5 ); } catch ( const std::invalid_argument & ) { caught = true; }
    assert( caught );
    caught = false;
    try { g.release( xs[ 0 ] ); } catch ( const std::invalid_argument & ) { caught = true; }
    assert( !caught );   /* ‹xs[ 0 ]› is the block allocated by ‹alloc( 30 )› */
    caught = false;
    try { g.release( xs[ 2 ] ); } catch ( const std::invalid_argument & ) { caught = true; }
    assert( caught );    /* already free */

    return 0;
}
