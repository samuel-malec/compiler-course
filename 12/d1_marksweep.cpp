#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

/* This demo introduces the «object heap» which most of the exercises in
 * this chapter work with, and the simplest tracing garbage collector:
 * «mark & sweep» (McCarthy, 1960 – it was invented for the first Lisp).
 *
 * The heap is deliberately abstract: objects are entries in a vector
 * and references are indices. A real heap is a block of raw memory and
 * references are addresses (that is the model of ‹d2_copying›), but the
 * algorithms are the same, and indices make mistakes easy to detect:
 * every access through a reference checks that the object has not
 * been freed. */

/* The object heap of this chapter (see ‹d1_marksweep›). A heap is a
 * vector of objects, numbered by their index; each object has a vector
 * of «fields», and each field holds a ‹value›: an integer or a
 * «reference» to an object (‹num› and ‹ref› build them). The «roots» are
 * the values which the program can use directly – in a real system,
 * the registers, the stack and the global variables. An object which
 * is not reachable from the roots is «garbage».
 *
 * ‹alloc› creates an object, reusing the most recently freed slot from
 * the «free list» if there is one; ‹release› frees an object and puts it
 * onto the free list. ‹get› and ‹at› access a referenced object and its
 * fields; both throw ‹std::logic_error› if the reference is «dangling»
 * (its object was freed). ‹used› counts the objects which are not free,
 * and ‹sound› checks that no object reachable from the roots has been
 * freed – a collector which makes it return ‹false› has freed a live
 * object. Each object also has a ‹mark› bit for the collectors to use. */

struct value
{
    bool is_ref = false;
    long n = 0;   /* the integer, or the index of the referenced object */
    bool operator==( const value & ) const = default;
};

value num( long n ) { return { false, n }; }
value ref( long i ) { return { true, i }; }

struct object
{
    std::vector< value > fields;
    bool free = false;
    bool mark = false;
};

struct heap
{
    std::vector< object > objects;
    std::vector< value > roots;
    std::vector< long > free_list;

    long alloc( std::vector< value > fields )
    {
        if ( free_list.empty() )
        {
            objects.push_back( { std::move( fields ) } );
            return long( objects.size() ) - 1;
        }
        long i = free_list.back();
        free_list.pop_back();
        objects[ i ] = { std::move( fields ) };
        return i;
    }

    void release( long i )
    {
        if ( i < 0 || i >= long( objects.size() ) || objects[ i ].free )
            throw std::logic_error( "bad free" );
        objects[ i ].free = true;
        objects[ i ].fields.clear();
        free_list.push_back( i );
    }

    bool valid( value r ) const
    {
        return r.is_ref && r.n >= 0 && r.n < long( objects.size() ) && !objects[ r.n ].free;
    }

    object &get( value r )
    {
        if ( !valid( r ) ) throw std::logic_error( "dangling reference" );
        return objects[ r.n ];
    }

    value &at( value r, std::size_t k )
    {
        auto &o = get( r );
        if ( k >= o.fields.size() ) throw std::out_of_range( "no such field" );
        return o.fields[ k ];
    }

    long used() const
    {
        return std::count_if( objects.begin(), objects.end(), []( const object &o ) { return !o.free; } );
    }

    bool sound() const
    {
        std::vector< bool > seen( objects.size() );
        std::vector< value > todo = roots;
        while ( !todo.empty() )
        {
            value v = todo.back();
            todo.pop_back();
            if ( !v.is_ref ) continue;
            if ( !valid( v ) ) return false;
            if ( seen[ v.n ] ) continue;
            seen[ v.n ] = true;
            for ( auto f : objects[ v.n ].fields ) todo.push_back( f );
        }
        return true;
    }
};

/* The «mark» phase: every object reachable from the roots gets its
 * mark bit set. The traversal is a depth-first search; it is
 * recursive here, which is short and clear, but a long linked list
 * makes the recursion as deep as the list – real collectors use an
 * explicit «mark stack» (‹e2_mark›) or pointer reversal (‹p1_dsw›),
 * since the collector usually runs exactly when memory is scarce. */

void mark( heap &h, value v )
{
    if ( !v.is_ref ) return;
    object &o = h.get( v );
    if ( o.mark ) return;   /* already visited: this also handles cycles */
    o.mark = true;
    for ( auto f : o.fields )
        mark( h, f );
}

/* The «sweep» phase: walk over the whole heap, free every unmarked
 * object, and clear the marks for the next collection. Note that the
 * cost of sweeping is proportional to the size of the «heap», while
 * the cost of marking is proportional to the amount of «live» data. */

long sweep( heap &h )
{
    long freed = 0;
    for ( long i = 0; i < long( h.objects.size() ); ++ i )
    {
        object &o = h.objects[ i ];
        if ( o.free ) continue;
        if ( o.mark ) o.mark = false;
        else h.release( i ), ++ freed;
    }
    return freed;
}

long collect( heap &h )
{
    for ( auto r : h.roots )
        mark( h, r );
    return sweep( h );
}

/* A linked list in the heap: each cell has two fields, the value and
 * the next cell (or the integer 0 at the end). */

value cons( heap &h, value car, value cdr ) { return ref( h.alloc( { car, cdr } ) ); }

value list( heap &h, long from, long to )
{
    value l = num( 0 );
    for ( long i = to; i >= from; -- i )
        l = cons( h, num( i ), l );
    return l;
}

long sum( heap &h, value l )
{
    long s = 0;
    for ( ; l.is_ref; l = h.at( l, 1 ) )
        s += h.at( l, 0 ).n;
    return s;
}

int main()
{
    heap h;

    /* two lists; only the first is a root */
    value a = list( h, 1, 10 );
    list( h, 1, 5 );
    h.roots = { a };
    assert( h.used() == 15 );

    assert( collect( h ) == 5 );
    assert( h.used() == 10 );
    assert( h.sound() );
    assert( sum( h, a ) == 55 );

    /* the freed cells are reused: allocation takes them from the free
     * list before the heap grows */
    list( h, 1, 3 );
    assert( h.objects.size() == 15 );
    assert( collect( h ) == 3 );

    /* dropping a reference turns a part of the list into garbage:
     * cut the list after its third cell */
    h.at( h.at( h.at( a, 1 ), 1 ), 1 ) = num( 0 );
    assert( collect( h ) == 7 );
    assert( sum( h, a ) == 6 );

    /* a cycle is collected as soon as it becomes unreachable (this is
     * what reference counting cannot do, see ‹e1_refcount›) */
    value x = cons( h, num( 1 ), num( 0 ) ), y = cons( h, num( 2 ), x );
    h.at( x, 1 ) = y;
    h.roots.push_back( x );
    assert( collect( h ) == 0 );
    h.roots.pop_back();
    assert( collect( h ) == 2 );
    assert( h.used() == 3 );
    assert( h.sound() );

    /* a dangling reference is detected */
    bool caught = false;
    try { h.at( x, 0 ); } catch ( const std::logic_error & ) { caught = true; }
    assert( caught );

    /* a collector which forgets a root is caught by ‹sound› */
    h.roots.push_back( list( h, 1, 3 ) );
    h.release( h.roots.back().n );
    assert( !h.sound() );

    return 0;
}
