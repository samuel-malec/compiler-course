#include <algorithm>
#include <cassert>
#include <utility>
#include <vector>

/* When the compiler gives the collector no help at all – no tags, no
 * stack maps – as with C and C++ programs, the collector can still work
 * «conservatively» (Boehm and Weiser, 1988): «any» word which looks like
 * a pointer to an allocated object is assumed to be one. Some objects
 * are then retained although they are garbage (an integer happened to
 * look like their address – «false retention»), but no live object is
 * ever freed. Such a collector cannot move objects (it cannot update a
 * ‘pointer’ which may be an integer), so it is a mark & sweep.
 *
 * Write ‹conservative›, which returns the start addresses (sorted) of
 * the objects which must be retained. The memory is a vector of words;
 * the allocated objects are given as pairs (start, size) – they do not
 * overlap, but they are given in no particular order; the roots are
 * the words of the ‹stack›. A word ‹w› refers to an object if
 *
 *  • ‹w› is equal to the start of the object, or
 *  • ‹interior› is true and ‹start ≤ w < start + size› («interior
 *    pointers» – C code, and optimizing compilers, routinely keep
 *    pointers into the middle of objects).
 *
 * All words of a retained object are scanned in the same way. The
 * memory, the number of objects and the stack may be large: finding
 * the object a word refers to must be fast (sort the objects once). */

// std::vector< std::size_t > conservative( const std::vector< long > &mem,
//                                          std::vector< std::pair< std::size_t, std::size_t > > objects,
//                                          const std::vector< long > &stack, bool interior );

std::vector< std::size_t > conservative( const std::vector< long > &mem,
                                         std::vector< std::pair< std::size_t, std::size_t > > objects,
                                         const std::vector< long > &stack, bool interior )
{
    std::sort( objects.begin(), objects.end() );
    std::vector< bool > marked( objects.size() );
    std::vector< std::size_t > todo;   /* indices into ‹objects› */

    auto consider = [ & ]( long w )
    {
        if ( w < 0 ) return;
        std::size_t a = w;
        /* the last object which starts at or before ‹a› */
        auto it = std::upper_bound( objects.begin(), objects.end(), std::pair( a, std::size_t( -1 ) ) );
        if ( it == objects.begin() ) return;
        -- it;
        auto [ start, size ] = *it;
        if ( interior ? a >= start + size : a != start ) return;
        std::size_t k = it - objects.begin();
        if ( !marked[ k ] ) marked[ k ] = true, todo.push_back( k );
    };

    for ( long w : stack ) consider( w );
    while ( !todo.empty() )
    {
        auto [ start, size ] = objects[ todo.back() ];
        todo.pop_back();
        for ( std::size_t a = start; a < start + size && a < mem.size(); ++ a ) consider( mem[ a ] );
    }

    std::vector< std::size_t > out;
    for ( std::size_t k = 0; k < objects.size(); ++ k )
        if ( marked[ k ] ) out.push_back( objects[ k ].first );
    return out;
}

int main()
{
    using v = std::vector< std::size_t >;
    std::vector< long > mem( 40, -1 );
    std::vector< std::pair< std::size_t, std::size_t > > objs = { { 20, 3 }, { 0, 4 }, { 30, 10 }, { 10, 5 }, { 4, 2 } };
    mem[ 1 ] = 10;    /* a pointer to the start of the object at 10 */
    mem[ 12 ] = 22;   /* an interior pointer into the object at 20 */
    mem[ 21 ] = 7;    /* points between objects */
    mem[ 35 ] = 4;    /* the object at 30 refers to the one at 4 */
    std::vector< long > stack = { 0, 1234, -5, 33 };

    assert(( conservative( mem, objs, stack, true ) == v{ 0, 4, 10, 20, 30 } ));
    assert(( conservative( mem, objs, stack, false ) == v{ 0, 10 } ));
    assert(( conservative( mem, objs, {}, true ) == v{} ));

    /* false retention: an integer on the stack which looks like an
     * address keeps a whole structure alive */
    assert(( conservative( mem, objs, { 31 }, true ) == v{ 4, 30 } ));
    /* the end of an object is not a part of it */
    assert(( conservative( mem, objs, { 6, 40 }, true ) == v{} ));

    /* a long chain of interior pointers, objects given in reverse */
    const std::size_t n = 100000;
    std::vector< long > big( 3 * n + 1, -1 );
    std::vector< std::pair< std::size_t, std::size_t > > bobj;
    for ( std::size_t i = n; i-- > 0; )
    {
        bobj.emplace_back( 3 * i + 1, 3 );
        if ( i + 1 < n ) big[ 3 * i + 2 ] = 3 * ( i + 1 ) + 3;   /* the last word of the next one */
    }
    auto all = conservative( big, bobj, { 3 * 50000 + 1 }, true );
    assert( all.size() == n - 50000 && all.front() == 3 * 50000 + 1 && all.back() == 3 * ( n - 1 ) + 1 );
    assert(( conservative( big, bobj, { 3 * 50000 + 1 }, false ) == v{ 3 * 50000 + 1 } ));

    return 0;
}
