#include <algorithm>
#include <cassert>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

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

/* Two features let a program interact with the garbage collector:
 *
 *  • a «weak reference» does not keep its target alive: if the target
 *    is otherwise unreachable, it is collected and the weak reference is
 *    «cleared» (caches and canonicalizing tables are the typical use;
 *    ‹std::weak_ptr› is the reference-counting equivalent),
 *  • a «finalizer» is code which runs after its object has become
 *    unreachable, but before it is freed (to release an external
 *    resource, say). The finalizer can access the object – and
 *    everything it references – so these must survive the collection
 *    («resurrection»); the object is freed by a later collection, if it
 *    is still unreachable by then. A finalizer runs at most once.
 *
 * Write a mark & sweep ‹collect› with both. Weak references are
 * fields: the set ‹weak› lists the pairs (object, field index) which
 * are weak. ‹finalizable› lists the objects which have a finalizer that
 * has not yet run. The collection proceeds as follows (these are the
 * semantics of Java's weak references and finalizers):
 *
 *  1. mark from the roots, not following weak fields,
 *  2. the objects in ‹finalizable› which are unmarked are removed from
 *     it and returned in ‹finalize› (in their order in ‹finalizable›),
 *  3. mark from the objects in ‹finalize› (they and everything they
 *     reference survive this collection),
 *  4. in every marked object, clear (set to ‹num( 0 )›) each weak field
 *     whose target was «not» marked in step 1 – even if it was
 *     resurrected in step 3,
 *  5. free the unmarked objects (in increasing order), clear the marks.
 *
 * Return the number of freed objects and the objects to finalize. */

struct gc_result
{
    long freed = 0;
    std::vector< long > finalize;
};

// gc_result collect( heap &h, const std::set< std::pair< long, std::size_t > > &weak,
//                    std::vector< long > &finalizable );

int main()
{
    heap h;
    using fields = std::set< std::pair< long, std::size_t > >;

    value cache = ref( h.alloc( { num( 0 ), num( 0 ) } ) );
    value big = ref( h.alloc( { num( 1 ) } ) );
    value kept = ref( h.alloc( { num( 2 ) } ) );
    h.at( cache, 0 ) = big;
    h.at( cache, 1 ) = kept;
    h.roots = { cache, kept };
    fields weak = { { cache.n, 0 }, { cache.n, 1 } };
    std::vector< long > fin;

    auto r = collect( h, weak, fin );
    assert( r.freed == 1 && r.finalize.empty() );
    assert( h.at( cache, 0 ) == num( 0 ) );   /* cleared */
    assert( h.at( cache, 1 ) == kept );       /* still reachable through a root */
    assert( h.sound() );

    /* an object with a finalizer, weakly referenced from the cache */
    value f = ref( h.alloc( { num( 0 ) } ) );
    value child = ref( h.alloc( { num( 4 ) } ) );
    h.at( f, 0 ) = child;
    h.at( cache, 0 ) = f;
    fin = { f.n };
    r = collect( h, weak, fin );
    assert( r.freed == 0 );
    assert(( r.finalize == std::vector< long >{ f.n } ) && fin.empty() );
    assert( h.at( cache, 0 ) == num( 0 ) );   /* cleared before finalization */
    assert( h.at( f, 0 ) == child );           /* the finalizer can use ‹child› */

    /* the finalizer has run: the next collection frees both */
    r = collect( h, weak, fin );
    assert( r.freed == 2 && r.finalize.empty() );

    /* reachable objects are not finalized */
    value g = ref( h.alloc( { num( 5 ) } ) );
    h.roots.push_back( g );
    fin = { g.n };
    r = collect( h, weak, fin );
    assert( r.freed == 0 && r.finalize.empty() && fin.size() == 1 );

    /* an object reachable only from a finalizable one survives, but the
     * weak references to it are cleared */
    value w = ref( h.alloc( { num( 6 ) } ) );
    value e = ref( h.alloc( { w } ) );
    h.at( cache, 1 ) = w;
    fin.push_back( e.n );
    r = collect( h, weak, fin );
    assert( r.freed == 0 );
    assert(( r.finalize == std::vector< long >{ e.n } ) && ( fin == std::vector< long >{ g.n } ));
    assert( h.at( cache, 1 ) == num( 0 ) );
    assert( h.at( e, 0 ) == w && h.get( w ).fields[ 0 ] == num( 6 ) );

    /* a weak field in an object which is itself garbage is irrelevant;
     * a weak field holding an integer is left alone */
    h.at( cache, 0 ) = num( 17 );
    r = collect( h, weak, fin );
    assert( r.freed == 2 && h.at( cache, 0 ) == num( 17 ) );
    assert( h.sound() );

    return 0;
}
