#include <algorithm>
#include <cassert>
#include <set>
#include <stdexcept>
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

/* Most objects die young: a typical program allocates many short-lived
 * temporaries, and a few long-lived structures. A «generational»
 * collector exploits this by dividing the heap into a «young»
 * generation (the «nursery»), where new objects are allocated, and an
 * «old» generation. A «minor» collection looks at the nursery only –
 * it is cheap because most young objects are dead, and the old
 * objects are not traced at all. Objects which survive are «promoted»
 * to the old generation. A «major» collection, done rarely, collects
 * the whole heap.
 *
 * The catch: a young object may be referenced from an «old» one, and a
 * minor collection does not trace old objects. So every store of a
 * reference into an object passes through a «write barrier», which
 * records old objects that have been given a reference to a young one
 * in a «remembered set»; their fields are additional roots for the
 * minor collection.
 *
 * Implement the class ‹generational› (the heap is the public member
 * ‹h›; the program manipulates ‹h.roots› directly, but stores into
 * fields only with ‹write›):
 *
 *  • ‹alloc( fields )› – allocate a young object (in ‹h›), return its
 *    index,
 *  • ‹write( obj, k, v )› – store ‹v› into field ‹k› of ‹obj›, with the
 *    write barrier,
 *  • ‹minor()› – mark the young objects reachable from the roots and
 *    from the fields of the remembered objects, tracing through young
 *    objects only; free the unmarked young objects, promote the marked
 *    ones and empty the remembered set; return the number of freed
 *    objects,
 *  • ‹major()› – mark & sweep the whole heap; the survivors are all
 *    old, the remembered set is emptied; return the number of freed
 *    objects,
 *  • ‹scanned()› – the number of objects marked by the last collection,
 *  • ‹is_old( i )› – whether object ‹i› is in the old generation.
 *
 * Use explicit mark stacks (no recursion). Keep in mind that ‹h.alloc›
 * may reuse the slot of a freed old object. */

// class generational;

class generational
{
    std::vector< bool > _old;
    std::set< long > _remembered;
    long _scanned = 0;

    void grow() { _old.resize( h.objects.size(), false ); }

    /* mark from ‹start›, following references to objects for which
     * ‹follow› holds */
    template< typename F >
    void mark( std::vector< value > start, F follow )
    {
        while ( !start.empty() )
        {
            value v = start.back();
            start.pop_back();
            if ( !v.is_ref || !follow( v.n ) ) continue;
            object &o = h.get( v );
            if ( o.mark ) continue;
            o.mark = true;
            ++ _scanned;
            for ( auto f : o.fields ) start.push_back( f );
        }
    }

    long sweep( bool young_only )
    {
        long freed = 0;
        for ( long i = 0; i < long( h.objects.size() ); ++ i )
        {
            object &o = h.objects[ i ];
            if ( o.free || ( young_only && _old[ i ] ) ) continue;
            if ( o.mark ) o.mark = false, _old[ i ] = true;
            else h.release( i ), ++ freed;
        }
        _remembered.clear();
        return freed;
    }

public:
    heap h;

    long alloc( std::vector< value > fields )
    {
        long i = h.alloc( std::move( fields ) );
        grow();
        _old[ i ] = false;
        return i;
    }

    void write( value obj, std::size_t k, value v )
    {
        h.at( obj, k ) = v;
        if ( _old[ obj.n ] && v.is_ref && !_old[ v.n ] )
            _remembered.insert( obj.n );
    }

    long minor()
    {
        _scanned = 0;
        std::vector< value > start = h.roots;
        for ( long r : _remembered )
            for ( auto f : h.objects[ r ].fields ) start.push_back( f );
        mark( start, [ & ]( long i ) { return !_old[ i ]; } );
        return sweep( true );
    }

    long major()
    {
        _scanned = 0;
        mark( h.roots, []( long ) { return true; } );
        return sweep( false );
    }

    long scanned() const { return _scanned; }
    bool is_old( long i ) const { return _old.at( i ); }
};

int main()
{
    generational g;

    /* a long-lived list of 100 cells */
    value l = num( 0 );
    for ( int i = 0; i < 100; ++ i ) l = ref( g.alloc( { num( i ), l } ) );
    g.h.roots = { l };
    assert( g.minor() == 0 && g.scanned() == 100 );
    assert( g.is_old( l.n ) );

    /* temporaries: a minor collection does not even look at the list */
    for ( int i = 0; i < 50; ++ i ) g.alloc( { num( i ) } );
    assert( g.minor() == 50 && g.scanned() == 0 );

    /* a young object referenced only from an old one */
    value y = ref( g.alloc( { num( 42 ) } ) );
    assert( !g.is_old( y.n ) );
    g.write( l, 0, y );
    assert( g.minor() == 0 && g.scanned() == 1 );
    assert( g.is_old( y.n ) && g.h.sound() );

    /* young objects referencing each other, reachable from a root */
    value z = ref( g.alloc( { num( 1 ) } ) );
    value w = ref( g.alloc( { z } ) );
    g.alloc( { w } );   /* garbage */
    g.h.roots.push_back( w );
    assert( g.minor() == 1 && g.scanned() == 2 );

    /* old garbage survives minor collections, but not a major one */
    g.write( l, 1, num( 0 ) );   /* cut off 99 cells */
    assert( g.minor() == 0 && g.scanned() == 0 );
    assert( g.major() == 99 && g.scanned() == 4 );
    assert( g.h.sound() );

    /* the freed slots are reused for young objects */
    value n = ref( g.alloc( { num( 7 ) } ) );
    assert( n.n < 100 && !g.is_old( n.n ) );
    g.write( w, 0, n );            /* ‹w› is old */
    g.write( w, 0, num( 0 ) );     /* … and drops it again: the remembered */
    assert( g.minor() == 1 );      /* set may be imprecise, but not unsafe */
    assert( g.h.sound() );

    return 0;
}
