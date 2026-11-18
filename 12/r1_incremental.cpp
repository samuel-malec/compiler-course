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

/* A mark & sweep collector stops the program for the whole
 * collection – with a large heap, that is a long «pause». An
 * «incremental» collector instead marks a little at a time, between
 * steps of the program (the «mutator»). The standard description uses
 * three colours (Dijkstra et al., 1978):
 *
 *  • «white» – not reached (yet); at the end, white objects are garbage,
 *  • «grey» – reached, but its fields have not been scanned,
 *  • «black» – reached and scanned.
 *
 * Marking starts with the roots grey; a marking step takes a grey
 * object, shades its white children grey, and turns it black. It is
 * done when nothing is grey. The danger is that the mutator changes
 * the graph in between: if it stores a reference to a white object
 * into a black one, and removes all other paths to the white object,
 * the collector never sees it and frees a live object. To prevent
 * this, every store goes through a «write barrier», which shades the
 * stored object grey if marking is in progress (Dijkstra's
 * «insertion» barrier).
 *
 * The roots are not protected by the barrier (stores to local
 * variables are too frequent for that); instead, the final step
 * rescans them, with the mutator stopped. Implement the class
 * ‹incremental›, working on the given heap (use ‹mark› for objects
 * which are grey or black, plus a list of grey objects):
 *
 *  • ‹start()› – begin marking: all objects white, the roots grey,
 *  • ‹step( n )› – blacken up to ‹n› grey objects; return ‹true› if no
 *    grey objects remain (also when marking is not in progress),
 *  • ‹write( obj, k, v )› – the mutator stores ‹v› into field ‹k› of
 *    ‹obj›, with the barrier,
 *  • ‹alloc( fields )› – the mutator allocates an object; while marking
 *    is in progress, new objects are black (they cannot be garbage
 *    yet) – but their initial fields are stores into a black object,
 *    so the barrier applies to them,
 *  • ‹finish()› – shade the roots, mark until nothing is grey, free the
 *    white objects, clear the marks and return the number of freed
 *    objects; marking is no longer in progress (if it was not in
 *    progress, ‹finish› performs a complete collection),
 *  • ‹marking()› – whether marking is in progress.
 *
 * Objects which become garbage during marking may survive until the
 * next cycle («floating garbage»). */

// class incremental;

int main()
{
    heap h;
    incremental gc( h );

    value c = ref( h.alloc( { num( 3 ) } ) );
    value b = ref( h.alloc( { c } ) );
    value a = ref( h.alloc( { b, num( 0 ) } ) );
    h.alloc( { num( 9 ) } );
    h.roots = { a };

    /* the classic race: ‹c› moves from the grey ‹b› to the black ‹a› */
    gc.start();
    assert( gc.marking() );
    assert( !gc.step( 1 ) );     /* ‹a› is black, ‹b› grey */
    gc.write( a, 1, c );
    gc.write( b, 0, num( 0 ) );
    assert( gc.step( 10 ) );
    assert( gc.finish() == 1 );
    assert( !gc.marking() && h.sound() );
    for ( const auto &o : h.objects ) assert( !o.mark );

    /* floating garbage */
    gc.start();
    assert( gc.step( 10 ) );
    gc.write( a, 0, num( 0 ) );   /* ‹b› is garbage now – but already black */
    assert( gc.finish() == 0 );
    assert( gc.finish() == 1 );   /* a complete collection */

    /* new objects are black */
    gc.start();
    value n = ref( gc.alloc( { c } ) );
    gc.write( a, 0, n );
    gc.write( a, 1, num( 0 ) );
    assert( gc.finish() == 0 && h.sound() );

    /* a reference moved into a root, which has already been scanned */
    gc.start();
    h.roots.push_back( h.at( a, 0 ) );   /* ‹n› */
    gc.write( a, 0, num( 0 ) );
    assert( gc.step( 10 ) );
    assert( gc.finish() == 0 && h.sound() );
    h.roots.pop_back();
    assert( gc.finish() == 2 );   /* ‹n› and ‹c› */

    /* a busy mutator: random stores of reachable references, interleaved
     * with marking steps */
    heap r;
    incremental rg( r );
    unsigned seed = 1;
    auto rnd = [ & ]( unsigned m ) { seed = seed * 1103515245 + 12345; return ( seed >> 8 ) % m; };
    for ( int i = 0; i < 300; ++ i ) r.alloc( { num( i ), num( 0 ), num( 0 ) } );
    for ( int i = 0; i < 300; ++ i ) r.objects[ i ].fields[ 1 + rnd( 2 ) ] = ref( rnd( 300 ) );
    r.roots = { ref( 0 ), ref( 1 ), ref( 2 ) };

    auto reachable = [ & ]
    {
        std::vector< long > out;
        std::vector< bool > seen( r.objects.size() );
        std::vector< value > todo = r.roots;
        while ( !todo.empty() )
        {
            value v = todo.back();
            todo.pop_back();
            if ( !v.is_ref || seen[ v.n ] ) continue;
            seen[ v.n ] = true;
            out.push_back( v.n );
            for ( auto f : r.objects[ v.n ].fields ) todo.push_back( f );
        }
        return out;
    };

    for ( int cycle = 0; cycle < 20; ++ cycle )
    {
        rg.start();
        do
        {
            for ( int k = 0; k < 5; ++ k )
            {
                auto live = reachable();
                value obj = ref( live[ rnd( live.size() ) ] );
                value v = rnd( 4 ) ? ref( live[ rnd( live.size() ) ] ) : num( 0 );
                if ( rnd( 10 ) == 0 ) v = ref( rg.alloc( { num( -1 ), v, num( 0 ) } ) );
                rg.write( obj, 1 + rnd( 2 ), v );
                if ( rnd( 20 ) == 0 ) r.roots[ rnd( 3 ) ] = ref( live[ rnd( live.size() ) ] );
            }
        } while ( !rg.step( 2 ) );
        rg.finish();
        assert( r.sound() );
    }

    return 0;
}
