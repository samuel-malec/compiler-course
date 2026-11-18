#include <cassert>
#include <stdexcept>
#include <vector>

/* In this exercise, we implement «reference counting». The heap is a
 * vector of objects; each object has fields which refer to other
 * objects by index (‹-1› is the null reference), a reference count,
 * and a flag which says whether it has been freed. */

struct rc_object
{
    std::vector< int > fields;
    int count = 0;
    bool freed = false;
};

using rc_heap = std::vector< rc_object >;

/* The count of an object is the number of references to it: from the
 * fields of other objects, and from the program's variables (which
 * the program reports by calling ‹incref› and ‹decref›). Implement:
 *
 *  • ‹make› – create an object with the given fields and return its
 *    index; its count is 1 (the reference which ‹make› returns), and
 *    the count of every object it refers to increases,
 *  • ‹incref› – add a reference to an object,
 *  • ‹decref› – remove a reference; when the count drops to zero, the
 *    object is freed: ‹freed› is set, its fields are cleared, and the
 *    references it held are removed (which may free further objects),
 *  • ‹assign› – store a reference into a field of an object (the old
 *    reference in the field is removed).
 *
 * Both ‹incref› and ‹decref› ignore ‹-1›; using a freed object in any of
 * the functions throws ‹std::logic_error›. Freeing must not use
 * recursion: dropping the last reference to a linked list of a
 * million objects must work (use an explicit list of objects to
 * process). Mind the order of operations in ‹assign› when the new and
 * the old reference are the same. */

// int make( rc_heap &h, std::vector< int > fields );
// void incref( rc_heap &h, int obj );
// void decref( rc_heap &h, int obj );
// void assign( rc_heap &h, int obj, std::size_t field, int target );

int main()
{
    rc_heap h;
    int a = make( h, {} );
    int b = make( h, { a, a } );
    assert( h[ a ].count == 3 && h[ b ].count == 1 );

    decref( h, a );   /* our variable ‹a› goes away; ‹b› still refers to it */
    assert( h[ a ].count == 2 && !h[ a ].freed );
    decref( h, b );
    assert( h[ b ].freed && h[ a ].freed );
    assert( h[ b ].fields.empty() );

    bool caught = false;
    try { incref( h, a ); } catch ( const std::logic_error & ) { caught = true; }
    assert( caught );

    /* assigning the reference which is already there */
    int x = make( h, {} );
    int y = make( h, { x } );
    decref( h, x );
    assign( h, y, 0, h[ y ].fields[ 0 ] );
    assert( !h[ x ].freed && h[ x ].count == 1 );
    assign( h, y, 0, -1 );
    assert( h[ x ].freed );
    decref( h, y );

    /* a long list */
    int list = -1;
    for ( int i = 0; i < 1000000; ++ i )
    {
        int cell = make( h, { list } );
        decref( h, list );   /* only the new cell refers to the old head now */
        list = cell;
    }
    assert( h[ list ].count == 1 && h[ h[ list ].fields[ 0 ] ].count == 1 );
    decref( h, list );
    for ( const auto &o : h ) assert( o.freed );

    /* the weakness of reference counting: a cycle is never freed */
    int p = make( h, { -1 } ), q = make( h, { p } );
    assign( h, p, 0, q );
    decref( h, p ), decref( h, q );
    assert( !h[ p ].freed && !h[ q ].freed );
    assert( h[ p ].count == 1 && h[ q ].count == 1 );

    return 0;
}
