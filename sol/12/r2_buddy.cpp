#include <cassert>
#include <optional>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>

/* The «buddy» allocator (Knowlton, 1965; used for physical pages by the
 * Linux kernel) manages a memory of ‹2ᵏ› words, and only ever hands out
 * blocks whose size is a power of two, at addresses which are
 * multiples of their size. A block of size ‹2ʲ› at address ‹a› can be
 * split into two «buddies» of size ‹2ʲ⁻¹›, at ‹a› and ‹a + 2ʲ⁻¹›; the
 * buddy of the block at ‹a› of size ‹s› is always at ‹a xor s›. This
 * makes merging trivial: when a block is freed and its buddy is free
 * (and of the same size), the two are merged, and the merging
 * continues with the bigger block.
 *
 * Implement the class ‹buddy›:
 *
 *  • ‹buddy( k )› – manage the addresses ‹0 … 2ᵏ - 1› (‹0 ≤ k < 40›),
 *    initially a single free block,
 *  • ‹alloc( n )› – round ‹n› up to a power of two ‹2ʲ›; find the
 *    smallest size ‹2ⁱ ≥ 2ʲ› which has a free block; take the free block
 *    of that size with the lowest address; while it is larger than
 *    ‹2ʲ›, split it, keeping the lower half and making the upper one
 *    free; return the address (or ‹std::nullopt› if there is no such
 *    block, or ‹n› is 0),
 *  • ‹release( addr )› – free the block allocated at ‹addr› and merge
 *    it with its buddies as long as possible; throw
 *    ‹std::invalid_argument› if no block is allocated at ‹addr›,
 *  • ‹free_blocks( j )› – the addresses of the free blocks of size
 *    ‹2ʲ›, in increasing order,
 *  • ‹largest()› – the size of the largest free block (0 if none).
 *
 * All operations should take time proportional to ‹k› (times a
 * logarithm, if you use ordered containers). */

// class buddy;

class buddy
{
    int _order;
    std::vector< std::set< std::size_t > > _free;   /* by order */
    std::map< std::size_t, int > _used;             /* address → order */

public:
    explicit buddy( int k ) : _order( k ), _free( k + 1 )
    {
        _free[ k ].insert( 0 );
    }

    std::optional< std::size_t > alloc( std::size_t n )
    {
        if ( n == 0 ) return std::nullopt;
        int j = 0;
        while ( ( std::size_t( 1 ) << j ) < n )
            if ( ++ j > _order ) return std::nullopt;

        int i = j;
        while ( i <= _order && _free[ i ].empty() ) ++ i;
        if ( i > _order ) return std::nullopt;

        std::size_t a = *_free[ i ].begin();
        _free[ i ].erase( _free[ i ].begin() );
        while ( i > j )
        {
            -- i;
            _free[ i ].insert( a + ( std::size_t( 1 ) << i ) );   /* the upper half */
        }
        _used[ a ] = j;
        return a;
    }

    void release( std::size_t addr )
    {
        auto u = _used.find( addr );
        if ( u == _used.end() ) throw std::invalid_argument( "not allocated" );
        int j = u->second;
        _used.erase( u );

        while ( j < _order )
        {
            std::size_t b = addr ^ ( std::size_t( 1 ) << j );
            auto it = _free[ j ].find( b );
            if ( it == _free[ j ].end() ) break;
            _free[ j ].erase( it );
            addr = std::min( addr, b );
            ++ j;
        }
        _free[ j ].insert( addr );
    }

    std::vector< std::size_t > free_blocks( int j ) const
    {
        if ( j < 0 || j > _order ) return {};
        return { _free[ j ].begin(), _free[ j ].end() };
    }

    std::size_t largest() const
    {
        for ( int j = _order; j >= 0; -- j )
            if ( !_free[ j ].empty() ) return std::size_t( 1 ) << j;
        return 0;
    }
};

int main()
{
    using v = std::vector< std::size_t >;
    buddy b( 4 );
    assert( b.largest() == 16 );

    auto x = b.alloc( 3 );   /* 16 → 8 + 8 → 4 + 4 + 8 */
    assert( x == 0 );
    assert( b.free_blocks( 2 ) == v{ 4 } && b.free_blocks( 3 ) == v{ 8 } );

    auto y = b.alloc( 1 );   /* the smallest free block is the 4 at 4 */
    assert( y == 4 );
    assert( b.free_blocks( 0 ) == v{ 5 } && b.free_blocks( 1 ) == v{ 6 } && b.free_blocks( 2 ).empty() );

    auto z = b.alloc( 5 );   /* rounded up to 8: internal fragmentation */
    assert( z == 8 && b.largest() == 2 );
    assert( !b.alloc( 3 ) );
    assert( !b.alloc( 0 ) );

    b.release( *y );         /* 1 at 4 + 1 at 5 → 2 at 4; + 2 at 6 → 4 at 4 */
    assert( b.free_blocks( 2 ) == v{ 4 } && b.free_blocks( 0 ).empty() && b.free_blocks( 1 ).empty() );
    b.release( *x );         /* → 8 at 0; its buddy (8 at 8) is in use */
    assert( b.free_blocks( 3 ) == v{ 0 } && b.largest() == 8 );

    bool caught = false;
    try { b.release( 0 ); } catch ( const std::invalid_argument & ) { caught = true; }
    assert( caught );
    caught = false;
    try { b.release( 9 ); } catch ( const std::invalid_argument & ) { caught = true; }
    assert( caught );

    b.release( *z );
    assert( b.largest() == 16 && b.free_blocks( 4 ) == v{ 0 } );
    assert( !b.alloc( 17 ) );
    assert( b.alloc( 16 ) == 0 && b.largest() == 0 );

    /* a stress test: blocks never overlap, and everything merges back */
    buddy big( 20 );
    std::vector< std::pair< std::size_t, std::size_t > > used;   /* address, size */
    std::vector< bool > taken( 1 << 20 );
    unsigned seed = 7;
    auto rnd = [ & ]( unsigned m ) { seed = seed * 1103515245 + 12345; return ( seed >> 8 ) % m; };
    for ( int i = 0; i < 20000; ++ i )
    {
        if ( used.empty() || rnd( 3 ) )
        {
            std::size_t n = 1 + rnd( 1 << rnd( 12 ) ), s = 1;
            while ( s < n ) s *= 2;
            auto a = big.alloc( n );
            if ( !a ) continue;
            assert( *a % s == 0 );
            for ( std::size_t k = *a; k < *a + s; ++ k ) assert( !taken[ k ] ), taken[ k ] = true;
            used.emplace_back( *a, s );
        }
        else
        {
            std::size_t j = rnd( used.size() );
            auto [ a, s ] = used[ j ];
            big.release( a );
            for ( std::size_t k = a; k < a + s; ++ k ) taken[ k ] = false;
            used.erase( used.begin() + j );
        }
    }
    for ( auto [ a, _ ] : used ) big.release( a );
    assert( big.largest() == 1 << 20 );

    return 0;
}
