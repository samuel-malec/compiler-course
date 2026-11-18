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

/* A collector runs when memory is short, so it should not need much
 * memory itself – but the mark stack of ‹e2_mark› can grow as large as
 * the heap. The Deutsch–Schorr–Waite algorithm (1967) marks the heap
 * with «no» stack at all: as it walks down a reference, it «reverses»
 * it – the field which it followed is overwritten with a reference to
 * the object it came from. The chain of reversed fields «is» the
 * stack, threaded through the objects themselves; on the way back up,
 * each field is restored to its original value.
 *
 * Write ‹mark_dsw›, which sets the ‹mark› bit of every object reachable
 * from the roots (processing the roots one by one) and leaves
 * everything else as it was: when it returns, all fields must hold
 * their original values. Besides the mark bits, it may use one small
 * counter per object – the index of the field which is currently being
 * visited (real implementations keep it in a few spare bits of the
 * object header); the vector ‹index› below is provided for that. It
 * must not use recursion, a stack, or any other data structure.
 *
 * Hint: keep two variables, ‹current› (the object being visited) and
 * ‹parent› (the object we came from, or none); when descending into
 * field ‹k› of ‹current›, the field takes the value of ‹parent›; when
 * retreating, the parent's field ‹index[ parent ]› tells where the
 * grandparent reference is stored, and is restored to ‹current›. */

// void mark_dsw( heap &h, std::vector< std::size_t > &index );

int main()
{
    auto check = []( heap &h )
    {
        auto before = h.objects;
        std::vector< std::size_t > index( h.objects.size() );
        mark_dsw( h, index );

        /* the expected marks, computed with an ordinary stack */
        std::vector< bool > reach( h.objects.size() );
        std::vector< value > todo = h.roots;
        while ( !todo.empty() )
        {
            value v = todo.back();
            todo.pop_back();
            if ( !v.is_ref || reach[ v.n ] ) continue;
            reach[ v.n ] = true;
            for ( auto f : h.objects[ v.n ].fields ) todo.push_back( f );
        }

        for ( std::size_t i = 0; i < h.objects.size(); ++ i )
        {
            if ( h.objects[ i ].mark != reach[ i ] ) return false;
            if ( h.objects[ i ].fields != before[ i ].fields ) return false;
        }
        return true;
    };

    heap h;
    auto obj = [ & ]( std::vector< value > f ) { return ref( h.alloc( std::move( f ) ) ); };

    value leaf = obj( { num( 1 ) } );
    value mid = obj( { leaf, num( 2 ), leaf } );
    value top = obj( { mid, obj( {} ), obj( { num( 3 ), mid } ) } );
    obj( { top } );   /* garbage */
    h.roots = { top, num( 5 ), leaf };
    assert( check( h ) );

    /* cycles, including an object which refers to itself */
    heap c;
    value a = ref( c.alloc( { num( 0 ), num( 0 ) } ) );
    value b = ref( c.alloc( { a, num( 0 ) } ) );
    c.at( a, 0 ) = b;
    c.at( a, 1 ) = a;
    c.at( b, 1 ) = b;
    c.alloc( { a } );
    c.roots = { b };
    assert( check( c ) );

    /* a long list, and a wide object */
    heap l;
    value list = num( 0 );
    for ( int i = 0; i < 100000; ++ i ) list = ref( l.alloc( { num( i ), list } ) );
    std::vector< value > many;
    for ( int i = 0; i < 1000; ++ i ) many.push_back( ref( l.alloc( { list } ) ) );
    l.roots = { ref( l.alloc( many ) ) };
    assert( check( l ) );

    return 0;
}
