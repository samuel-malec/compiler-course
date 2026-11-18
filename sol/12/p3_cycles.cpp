#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

/* Reference counting (‹e1_refcount›) cannot free cycles. A «cycle
 * collector» complements it; this exercise implements the synchronous
 * algorithm of Bacon and Rajan (2001), based on «trial deletion». It
 * is used, in variants, by CPython and PHP.
 *
 * The heap is like in ‹e1_refcount›, with two additions: a colour for
 * each object, and a buffer of «candidates». The given ‹decref› frees an
 * object whose count drops to zero (recursively, for simplicity); if
 * the count stays positive, the object may have become the root of a
 * garbage cycle, so it is remembered as a candidate. */

struct rc_object
{
    std::vector< int > fields;
    int count = 0;
    bool freed = false;
    char colour = 'B';      /* black, gray or white */
};

struct rc_heap
{
    std::vector< rc_object > objects;
    std::vector< int > candidates;

    int make( std::vector< int > fields )
    {
        for ( int f : fields ) incref( f );
        objects.push_back( { std::move( fields ), 1 } );
        return objects.size() - 1;
    }

    void incref( int i ) { if ( i != -1 ) ++ objects.at( i ).count; }

    void decref( int i )
    {
        if ( i == -1 ) return;
        rc_object &o = objects.at( i );
        if ( o.freed || o.count <= 0 ) throw std::logic_error( "bad decref" );
        if ( -- o.count > 0 )
        {
            if ( std::find( candidates.begin(), candidates.end(), i ) == candidates.end() )
                candidates.push_back( i );
            return;
        }
        o.freed = true;
        auto fields = std::move( o.fields );
        o.fields.clear();
        for ( int f : fields ) decref( f );
    }

    void assign( int obj, std::size_t k, int target )
    {
        incref( target );
        int old = objects.at( obj ).fields.at( k );
        objects[ obj ].fields[ k ] = target;
        decref( old );
    }
};

/* Write ‹collect_cycles›, which frees all garbage reachable from the
 * candidates and returns the indices of the freed objects, sorted. The
 * idea: the references which come from «inside» a group of objects do
 * not keep the group alive; if we subtract them, whatever still has a
 * positive count is referenced from outside (from the program, or a
 * live object).
 *
 *  1. «mark gray»: from each (not freed) candidate, traverse the graph;
 *     colour each visited object gray, and for every reference from a
 *     gray object to another object, decrement the count of the target
 *     (each reference once – an object is traversed only when it first
 *     turns gray),
 *  2. «scan»: from each candidate, traverse the gray objects again:
 *     an object whose count is still positive is externally reachable
 *     – «scan black» it: colour it, and everything reachable from it,
 *     black again, «restoring» the counts decremented in step 1 (each
 *     reference from a re-blackened object increments its target, and
 *     a gray target is blackened, too); an object with count zero turns
 *     white (for now – it may be blackened later), and the scan
 *     continues to its children,
 *  3. «collect white»: free every white object reachable from the
 *     candidates (set ‹freed›, clear the fields; no count changes are
 *     needed, as the internal references have already been subtracted)
 *     and colour it black.
 *
 * Afterwards, the candidate buffer is empty, all surviving objects are
 * black and their counts are exactly the number of references to them
 * again. Recursion is allowed. */

// std::vector< int > collect_cycles( rc_heap &h );

void mark_gray( rc_heap &h, int i )
{
    rc_object &o = h.objects[ i ];
    if ( o.colour == 'G' ) return;
    o.colour = 'G';
    for ( int f : o.fields )
        if ( f != -1 )
        {
            -- h.objects[ f ].count;
            mark_gray( h, f );
        }
}

void scan_black( rc_heap &h, int i )
{
    rc_object &o = h.objects[ i ];
    o.colour = 'B';
    for ( int f : o.fields )
        if ( f != -1 )
        {
            ++ h.objects[ f ].count;
            if ( h.objects[ f ].colour != 'B' ) scan_black( h, f );
        }
}

void scan( rc_heap &h, int i )
{
    rc_object &o = h.objects[ i ];
    if ( o.colour != 'G' ) return;
    if ( o.count > 0 ) return scan_black( h, i );
    o.colour = 'W';
    for ( int f : o.fields )
        if ( f != -1 ) scan( h, f );
}

void collect_white( rc_heap &h, int i, std::vector< int > &freed )
{
    rc_object &o = h.objects[ i ];
    if ( o.colour != 'W' ) return;
    o.colour = 'B';
    o.freed = true;
    freed.push_back( i );
    auto fields = std::move( o.fields );
    o.fields.clear();
    for ( int f : fields )
        if ( f != -1 ) collect_white( h, f, freed );
}

std::vector< int > collect_cycles( rc_heap &h )
{
    std::vector< int > roots;
    for ( int c : h.candidates )
        if ( !h.objects[ c ].freed ) roots.push_back( c );
    h.candidates.clear();

    for ( int c : roots ) mark_gray( h, c );
    for ( int c : roots ) scan( h, c );
    std::vector< int > freed;
    for ( int c : roots ) collect_white( h, c, freed );

    std::sort( freed.begin(), freed.end() );
    return freed;
}

int main()
{
    using v = std::vector< int >;

    /* a garbage cycle of three objects */
    rc_heap h;
    int a = h.make( { -1 } ), b = h.make( { a } ), c = h.make( { b } );
    h.assign( a, 0, c );
    h.decref( a ), h.decref( b ), h.decref( c );
    assert( !h.objects[ a ].freed );
    assert(( collect_cycles( h ) == v{ 0, 1, 2 } ));
    assert( h.candidates.empty() );

    /* the same cycle, still referenced by the program: nothing to free,
     * the counts are restored */
    rc_heap g;
    a = g.make( { -1 } ), b = g.make( { a } ), c = g.make( { b } );
    g.assign( a, 0, c );
    g.decref( a ), g.decref( c );   /* the program still holds ‹b› */
    assert( collect_cycles( g ).empty() );
    for ( int i : { a, b, c } ) assert( !g.objects[ i ].freed && g.objects[ i ].colour == 'B' );
    assert( g.objects[ a ].count == 1 && g.objects[ b ].count == 2 && g.objects[ c ].count == 1 );
    g.decref( b );
    assert(( collect_cycles( g ) == v{ 0, 1, 2 } ));

    /* a garbage cycle with a garbage tail, and a reference from the cycle
     * to a live object: the tail is freed, the live object loses one
     * reference */
    rc_heap t;
    int live = t.make( {} );
    int x = t.make( { -1, live } ), y = t.make( { x, -1 } );
    t.assign( x, 0, y );
    int tail = t.make( {} );
    t.assign( y, 1, tail );
    t.decref( tail );
    t.decref( x ), t.decref( y );
    assert( t.objects[ live ].count == 2 );
    assert(( collect_cycles( t ) == v{ x, y, tail } ));
    assert( !t.objects[ live ].freed && t.objects[ live ].count == 1 );

    /* a live object referenced from inside the group, which itself
     * refers back into it: it re-blackens the part of the group it
     * reaches */
    rc_heap r;
    int p = r.make( { -1 } ), q = r.make( { p } ), s = r.make( { q } );   /* s → q → p */
    r.assign( p, 0, q );                                                  /* p ↔ q */
    r.decref( p ), r.decref( q );   /* the program holds only ‹s› */
    assert( collect_cycles( r ).empty() );
    assert( r.objects[ p ].count == 1 && r.objects[ q ].count == 2 && r.objects[ s ].count == 1 );
    r.decref( s );                  /* frees ‹s›; ‹q› becomes a candidate */
    assert(( collect_cycles( r ) == v{ p, q } ));

    return 0;
}
