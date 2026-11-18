#include <algorithm>
#include <cassert>
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

/* Write a mark & sweep collector ‹collect› for the heap above, like the
 * one in ‹d1_marksweep›, but without recursion: the mark phase must
 * use an explicit «mark stack» (a vector of objects which have been
 * marked, but whose fields have not been looked at yet). The sweep
 * phase frees the unmarked objects in the order of increasing index
 * and clears all marks. Return the number of freed objects. */

// long collect( heap &h );

long collect( heap &h )
{
    std::vector< long > stack;
    auto shade = [ & ]( value v )
    {
        if ( !v.is_ref ) return;
        object &o = h.get( v );
        if ( !o.mark ) o.mark = true, stack.push_back( v.n );
    };

    for ( auto r : h.roots ) shade( r );
    while ( !stack.empty() )
    {
        long i = stack.back();
        stack.pop_back();
        for ( auto f : h.objects[ i ].fields ) shade( f );
    }

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

int main()
{
    heap h;
    auto cons = [ & ]( value a, value b ) { return ref( h.alloc( { a, b } ) ); };

    /* a tree with some garbage under it */
    value leaf = cons( num( 1 ), num( 2 ) );
    value t = cons( leaf, cons( leaf, cons( num( 3 ), num( 4 ) ) ) );
    cons( t, t );
    cons( num( 0 ), num( 0 ) );
    h.roots = { t, num( 7 ) };
    assert( h.used() == 6 );
    assert( collect( h ) == 2 );
    assert( h.used() == 4 && h.sound() );
    for ( const auto &o : h.objects ) assert( !o.mark );

    /* freed in increasing order, so the slot with the highest index is
     * reused first */
    assert( h.alloc( {} ) == 5 );
    assert( collect( h ) == 1 );

    /* a cycle reachable through a root, and one which is not */
    value c1 = cons( num( 1 ), num( 0 ) ), c2 = cons( num( 2 ), c1 );
    h.at( c1, 1 ) = c2;
    value d1 = cons( num( 1 ), num( 0 ) );
    h.at( d1, 1 ) = cons( num( 2 ), d1 );
    h.roots.push_back( c2 );
    assert( collect( h ) == 2 );
    assert( h.sound() );

    /* a deep list: a recursive mark would run out of stack */
    value l = num( 0 );
    for ( int i = 0; i < 200000; ++ i ) l = cons( num( i ), l );
    h.roots = { l };
    assert( collect( h ) == 6 );
    assert( h.used() == 200000 );
    h.roots.clear();
    assert( collect( h ) == 200000 );
    assert( h.used() == 0 );

    return 0;
}
