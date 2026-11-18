#include <cassert>
#include <optional>
#include <string>
#include <vector>

/* This demo introduces the second model of the heap used in this
 * chapter – «flat memory», a vector of words, closer to what a real
 * runtime system works with – and a «copying» collector (Fenichel and
 * Yochelson, 1969; Cheney, 1970).
 *
 * A copying collector divides memory into two «semispaces». The
 * program allocates in one of them («from-space») with a bump pointer.
 * When it is full, the collector copies every live object into the
 * other («to-space»), updates all pointers to refer to the copies, and
 * the spaces swap roles. Garbage is never even looked at: the work is
 * proportional to the live data. The copies are contiguous, so the
 * new space is compacted and allocation can continue with a bump
 * pointer. The price is that half of the memory is unused at any
 * time. */

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

/* When an object is copied, its header in from-space is overwritten
 * by a «forwarding pointer» – here, the negative number ‹-1 - b›, where
 * ‹b› is the address of the copy (headers are never negative
 * otherwise). Every other pointer to the same object is then
 * redirected to the same copy: this preserves sharing and handles
 * cycles.
 *
 * This version copies depth-first and recursively, which is simple,
 * but the recursion is as deep as the longest chain of pointers – a
 * problem which Cheney's algorithm solves elegantly (‹p2_cheney›). */

word copy( space &from, space &to, word w )
{
    if ( !is_ptr( w ) ) return w;
    long a = untag( w );
    if ( from.mem[ a ] < 0 ) return tag_ptr( -1 - from.mem[ a ] );   /* already copied */

    long n = from.mem[ a ];
    long b = to.top;
    to.mem[ b ] = n;
    to.top += n + 1;             /* reserve the space for the copy first */
    from.mem[ a ] = -1 - b;      /* … and forward the original */

    for ( long k = 1; k <= n; ++ k )
        to.mem[ b + k ] = copy( from, to, from.mem[ a + k ] );
    return tag_ptr( b );
}

/* Collect: copy everything reachable from the roots (which are
 * updated in place) and return the new space. */

space collect( space &from, std::vector< word > &roots )
{
    space to( from.mem.size() );
    for ( auto &r : roots )
        r = copy( from, to, r );
    return to;
}

/* The ‘program’: build lists of numbers, with a collection whenever an
 * allocation fails. The local variables of the program which hold
 * pointers must be roots, and they must be updated when the objects
 * move – that is why ‹cons› takes them by reference, through the root
 * vector. */

struct machine
{
    space heap;
    std::vector< word > roots;
    int collections = 0;

    explicit machine( std::size_t size ) : heap( size ) {}

    /* ‹roots[ car ]› and ‹roots[ cdr ]› are the fields of the new cell */
    word cons( std::size_t car, std::size_t cdr )
    {
        auto a = heap.alloc( { roots[ car ], roots[ cdr ] } );
        if ( !a )
        {
            heap = collect( heap, roots );
            ++ collections;
            a = heap.alloc( { roots[ car ], roots[ cdr ] } );
            assert( a );   /* out of memory */
        }
        return tag_ptr( *a );
    }
};

int main()
{
    space s( 100 );
    word one = tag_ptr( *s.alloc( { tag_int( 1 ), tag_int( 0 ) } ) );
    word two = tag_ptr( *s.alloc( { tag_int( 2 ), one } ) );
    s.alloc( { tag_int( 7 ), tag_int( 7 ), tag_int( 7 ) } );   /* garbage */
    word top = tag_ptr( *s.alloc( { two, two } ) );           /* shared */
    assert( s.top == 13 );
    assert( show( s, top ) == "((2 (1 0)) (2 (1 0)))" );

    std::vector< word > roots = { top, tag_int( 5 ) };
    space t = collect( s, roots );
    assert( t.top == 9 );   /* 3 objects of 2 fields: the garbage is gone */
    assert( show( t, roots[ 0 ] ) == "((2 (1 0)) (2 (1 0)))" );
    assert( roots[ 1 ] == tag_int( 5 ) );
    /* sharing is preserved: both fields point to the same copy */
    long r = untag( roots[ 0 ] );
    assert( t.mem[ r + 1 ] == t.mem[ r + 2 ] );

    /* a cycle */
    space c( 20 );
    long x = *c.alloc( { tag_int( 1 ), tag_int( 0 ) } );
    long y = *c.alloc( { tag_int( 2 ), tag_ptr( x ) } );
    c.mem[ x + 2 ] = tag_ptr( y );
    std::vector< word > cr = { tag_ptr( y ) };
    space d = collect( c, cr );
    assert( d.top == 6 );
    assert( show( d, cr[ 0 ], 4 ) == "(2 (1 (2 (1 …))))" );

    /* a program which allocates much more than the heap can hold: the
     * list of the last 10 numbers is live, everything else is garbage */
    machine m( 90 );
    m.roots = { tag_int( 0 ), tag_int( 0 ), tag_int( 0 ) };   /* list, number, scratch */
    for ( int i = 1; i <= 1000; ++ i )
    {
        m.roots[ 1 ] = tag_int( i );
        m.roots[ 0 ] = m.cons( 1, 0 );
        /* keep only 10 cells: cut the list after the 10th */
        word l = m.roots[ 0 ];
        for ( int k = 1; k < 10 && is_ptr( l ); ++ k )
            l = m.heap.mem[ untag( l ) + 2 ];
        if ( is_ptr( l ) ) m.heap.mem[ untag( l ) + 2 ] = tag_int( 0 );
    }
    assert( show( m.heap, m.roots[ 0 ] ) == "(1000 (999 (998 (997 (996 (995 (994 (993 (992 (991 0))))))))))" );
    /* the first collection happens at the 31st allocation; each leaves 10
     * live cells (30 words), so 20 more cells fit before the next one */
    assert( m.collections == 49 );

    return 0;
}
