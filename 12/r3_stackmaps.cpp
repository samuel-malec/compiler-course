#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

/* The collectors of this chapter could always tell references from
 * integers – by a tag in every word, or by the type of the field. Many
 * runtimes (the JVM, .NET, Go, LLVM-based compilers) do not tag their
 * values: an integer and a pointer are both just machine words. The
 * collector then needs the help of the compiler: for every point in
 * the code where a collection may happen (a «safepoint»), the compiler
 * records a «stack map» – which local variables and which stack slots
 * hold references at that point. Only those are roots; and since the
 * collector moves objects, exactly those are updated.
 *
 * In this exercise, you compute stack maps for a stack machine in the
 * style of Chapter 9, by a data-flow analysis over the bytecode which
 * is very similar to the JVM's bytecode verifier (and ‹09/r2_verify›).
 * The machine (given below) has local slots, an operand stack, and a
 * heap of objects with integer and reference fields. The instructions
 * (‹t› is a jump target, an index into the program; jumping to the
 * end of the program stops it):
 *
 *  • ‹push n› – an integer; ‹null› – the null reference,
 *  • ‹load i›, ‹store i› – slots (which all start as the integer 0),
 *  • ‹pop›, ‹dup›,
 *  • ‹add›, ‹sub›, ‹lt› – on two integers, giving an integer,
 *  • ‹jz t› – pop an integer, jump if it is 0; ‹jump t›,
 *  • ‹jnull t› – pop a reference, jump if it is null,
 *  • ‹print› – pop an integer and print it,
 *  • ‹alloc n m› – pop ‹n› values and create an object with them as its
 *    fields (the deepest of them is field 0); bit ‹k› of the «layout
 *    mask» ‹m› says whether field ‹k› is a reference; push a reference
 *    to the new object; this is the «only» safepoint: if the heap is
 *    full, the machine collects garbage «before» popping the operands
 *    (so they are on the stack, and must be in the stack map),
 *  • ‹geti k›, ‹getr k› – pop a (non-null) reference, push its field ‹k›,
 *    which is an integer / a reference,
 *  • ‹seti k›, ‹setr k› – pop an integer / a reference, pop a reference,
 *    store the first into field ‹k› of the second.
 *
 * Write ‹stack_maps›, which computes the stack map of every «reachable»
 * ‹alloc› instruction. The analysis assigns to each slot and stack
 * position, at each instruction, a type: integer, reference, or
 * «conflict» – a slot which holds an integer on one path into an
 * instruction and a reference on another. A conflict is not a root (we
 * do not know what it is), so the program must never use it: the
 * operands of arithmetic, ‹jz›, ‹print›, ‹geti› … ‹setr›, ‹jnull› and
 * ‹alloc› must have the types the instruction requires (‹alloc› checks
 * its operands against the mask); ‹load›, ‹store›, ‹pop› and ‹dup›
 * accept anything. Return ‹std::nullopt› if the program is not valid:
 * a type error, a stack underflow, a jump outside of ‹0 … size›, a slot
 * outside of ‹0 … slots - 1›, or two paths reaching an instruction
 * with different stack heights. The map lists the slots and the stack
 * positions (0 is the bottom) which hold references, in increasing
 * order. Note that the map describes the state «before» the ‹alloc›.
 *
 * (A reference which is «dead» at the safepoint – never used again –
 * could be left out of the map, so that its object can be freed
 * earlier; combining the analysis with liveness (‹11/p1_liveness›) is
 * what optimizing compilers do. Here, list all references.) */

enum class op { push, null, load, store, pop, dup, add, sub, lt, jz, jump, jnull, print, alloc,
                geti, getr, seti, setr };

struct instr
{
    op o;
    long arg = 0, arg2 = 0;
};

using program = std::vector< instr >;

struct stack_map
{
    std::vector< int > slots, stack;
    bool operator==( const stack_map & ) const = default;
};

using stack_maps_t = std::map< std::size_t, stack_map >;   /* index of an ‹alloc› → its map */

/* The machine. Its values are plain ‹long›s – references are addresses
 * (‹-1› is null) – so it relies on the stack maps to find the roots.
 * The heap is a pair of semispaces with a copying collector; an object
 * at address ‹a› has its number of fields at ‹a›, its layout mask at
 * ‹a + 1› and the fields after that.
 *
 * To catch wrong stack maps, the machine «also» keeps a hidden tag for
 * every slot and stack position – as a debugging aid, which a real
 * machine would not have. A collection checks that every position the
 * map lists holds a reference (otherwise, it would ‘update’ an integer
 * and corrupt it), and marks every reference which the map does «not»
 * list as «stale»: its object has moved and the value is a dangling
 * address. Using a stale reference throws ‹std::logic_error›. With
 * ‹always_collect›, the machine collects at every ‹alloc›, which finds
 * bugs in all maps quickly. A failure of the program itself (null
 * reference, out of memory, a jump outside, …) throws
 * ‹std::runtime_error›. */

struct vm_result
{
    std::vector< long > output;
    int collections = 0;
};

vm_result execute( const program &p, int slots, const stack_maps_t &maps, std::size_t semispace,
                   bool always_collect = false )
{
    struct cell { long v; char tag; };   /* tag: 'i', 'r' or 's' (stale) */
    std::vector< cell > local( slots, { 0, 'i' } ), stack;
    std::vector< long > heap( semispace );
    std::size_t top = 0;
    vm_result res;
    auto fail = []( const char *why ) { throw std::runtime_error( why ); };

    auto pop = [ & ]
    {
        if ( stack.empty() ) fail( "stack underflow" );
        cell c = stack.back();
        stack.pop_back();
        return c;
    };
    auto pop_int = [ & ] { cell c = pop(); if ( c.tag != 'i' ) fail( "not an integer" ); return c.v; };
    auto check_ref = [ & ]( cell c )
    {
        if ( c.tag == 's' ) throw std::logic_error( "stale reference" );
        if ( c.tag != 'r' ) fail( "not a reference" );
        return c.v;
    };
    auto pop_obj = [ & ]
    {
        long a = check_ref( pop() );
        if ( a == -1 ) fail( "null reference" );
        return a;
    };
    auto field = [ & ]( long a, long k, bool is_ref ) -> long &
    {
        if ( k < 0 || k >= heap[ a ] ) fail( "no such field" );
        if ( bool( heap[ a + 1 ] >> k & 1 ) != is_ref ) fail( "wrong field type" );
        return heap[ a + 2 + k ];
    };

    auto collect = [ & ]( std::size_t pc )
    {
        auto m = maps.find( pc );
        if ( m == maps.end() ) throw std::logic_error( "no stack map" );
        std::vector< long > to( semispace );
        std::size_t to_top = 0;

        auto copy = [ & ]( long a ) -> long
        {
            if ( a == -1 ) return a;
            if ( heap[ a ] < 0 ) return -1 - heap[ a ];   /* forwarded */
            long size = heap[ a ] + 2, b = to_top;
            std::copy( heap.begin() + a, heap.begin() + a + size, to.begin() + b );
            to_top += size;
            heap[ a ] = -1 - b;
            return b;
        };
        auto root = [ & ]( cell &c )
        {
            if ( c.tag != 'r' ) throw std::logic_error( "the stack map lists a non-reference" );
            c.v = copy( c.v );
        };

        std::vector< bool > is_root_slot( local.size() ), is_root_stack( stack.size() );
        for ( int i : m->second.slots )
        {
            if ( i < 0 || i >= int( local.size() ) ) throw std::logic_error( "bad stack map" );
            root( local[ i ] ), is_root_slot[ i ] = true;
        }
        for ( int i : m->second.stack )
        {
            if ( i < 0 || i >= int( stack.size() ) ) throw std::logic_error( "bad stack map" );
            root( stack[ i ] ), is_root_stack[ i ] = true;
        }
        for ( std::size_t i = 0; i < local.size(); ++ i )
            if ( !is_root_slot[ i ] && local[ i ].tag == 'r' && local[ i ].v != -1 ) local[ i ].tag = 's';
        for ( std::size_t i = 0; i < stack.size(); ++ i )
            if ( !is_root_stack[ i ] && stack[ i ].tag == 'r' && stack[ i ].v != -1 ) stack[ i ].tag = 's';

        for ( std::size_t scan = 0; scan < to_top; )   /* Cheney (‹p2_cheney›) */
        {
            long n = to[ scan ], mask = to[ scan + 1 ];
            for ( long k = 0; k < n; ++ k )
                if ( mask >> k & 1 ) to[ scan + 2 + k ] = copy( to[ scan + 2 + k ] );
            scan += n + 2;
        }
        heap = std::move( to );
        top = to_top;
        ++ res.collections;
    };

    for ( std::size_t pc = 0, steps = 0; pc < p.size(); )
    {
        if ( ++ steps > 1000000 ) fail( "out of fuel" );
        const instr &i = p[ pc ];
        std::size_t next = pc + 1;
        auto jump = [ & ] { if ( i.arg < 0 || std::size_t( i.arg ) > p.size() ) fail( "bad jump" ); next = i.arg; };
        auto slot = [ & ]() -> cell & { if ( i.arg < 0 || i.arg >= slots ) fail( "bad slot" ); return local[ i.arg ]; };

        switch ( i.o )
        {
            case op::push:  stack.push_back( { i.arg, 'i' } ); break;
            case op::null:  stack.push_back( { -1, 'r' } ); break;
            case op::load:  stack.push_back( slot() ); break;
            case op::store: { cell c = pop(); slot() = c; break; }
            case op::pop:   pop(); break;
            case op::dup:   { cell c = pop(); stack.push_back( c ), stack.push_back( c ); break; }
            case op::add: case op::sub: case op::lt:
            {
                long b = pop_int(), a = pop_int();
                stack.push_back( { i.o == op::add ? a + b : i.o == op::sub ? a - b : a < b, 'i' } );
                break;
            }
            case op::jz:    if ( pop_int() == 0 ) jump(); break;
            case op::jump:  jump(); break;
            case op::jnull: if ( check_ref( pop() ) == -1 ) jump(); break;
            case op::print: res.output.push_back( pop_int() ); break;
            case op::geti:  { long a = pop_obj(); stack.push_back( { field( a, i.arg, false ), 'i' } ); break; }
            case op::getr:  { long a = pop_obj(); stack.push_back( { field( a, i.arg, true ), 'r' } ); break; }
            case op::seti:  { long v = pop_int(); long a = pop_obj(); field( a, i.arg, false ) = v; break; }
            case op::setr:  { long v = check_ref( pop() ); long a = pop_obj(); field( a, i.arg, true ) = v; break; }
            case op::alloc:
            {
                long n = i.arg, mask = i.arg2;
                if ( n < 0 || std::size_t( n ) > stack.size() ) fail( "stack underflow" );
                if ( always_collect || top + n + 2 > semispace ) collect( pc );
                if ( top + n + 2 > semispace ) fail( "out of memory" );
                long a = top;
                heap[ a ] = n, heap[ a + 1 ] = mask;
                for ( long k = 0; k < n; ++ k )
                {
                    cell c = stack[ stack.size() - n + k ];
                    if ( mask >> k & 1 ) heap[ a + 2 + k ] = check_ref( c );
                    else if ( c.tag != 'i' ) fail( "not an integer" );
                    else heap[ a + 2 + k ] = c.v;
                }
                top += n + 2;
                stack.resize( stack.size() - n );
                stack.push_back( { a, 'r' } );
                break;
            }
        }
        pc = next;
    }
    return res;
}

// std::optional< stack_maps_t > stack_maps( const program &p, int slots );

int main()
{
    using enum op;

    /* build a list of the numbers 0 … n - 1, then sum it */
    auto list = []( long n ) -> program
    {
        return { { null }, { store, 0 }, { push, 0 }, { store, 1 },
                 /*  4 */ { load, 1 }, { push, n }, { lt }, { jz, 17 },
                 /*  8 */ { load, 1 }, { load, 0 }, { alloc, 2, 0b10 }, { store, 0 },
                 /* 12 */ { load, 1 }, { push, 1 }, { add }, { store, 1 }, { jump, 4 },
                 /* 17 */ { push, 0 }, { store, 1 },
                 /* 19 */ { load, 0 }, { jnull, 30 },
                 /* 21 */ { load, 1 }, { load, 0 }, { geti, 0 }, { add }, { store, 1 },
                 /* 26 */ { load, 0 }, { getr, 1 }, { store, 0 }, { jump, 19 },
                 /* 30 */ { load, 1 }, { print } };
    };

    auto maps = stack_maps( list( 10 ), 2 );
    assert( maps );
    assert(( *maps == stack_maps_t{ { 10, { { 0 }, { 1 } } } } ));

    auto r = execute( list( 10 ), 2, *maps, 1000, true );
    assert(( r.output == std::vector< long >{ 45 } ) && r.collections == 10 );
    r = execute( list( 100 ), 2, *stack_maps( list( 100 ), 2 ), 400, true );
    assert(( r.output == std::vector< long >{ 4950 } ) && r.collections == 100 );

    /* the machine catches wrong maps: a missing reference (the list
     * which is being extended), and an integer listed as a reference */
    bool caught = false;
    try { execute( list( 10 ), 2, { { 10, { { 0 }, {} } } }, 1000, true ); }
    catch ( const std::logic_error & ) { caught = true; }
    assert( caught );
    caught = false;
    try { execute( list( 10 ), 2, { { 10, { { 0, 1 }, { 1 } } } }, 1000, true ); }
    catch ( const std::logic_error & ) { caught = true; }
    assert( caught );

    /* nested objects; the operands of ‹alloc› are roots */
    program nest = { { push, 1 }, { alloc, 1, 0 }, { store, 0 },
                     { load, 0 }, { push, 5 }, { alloc, 2, 0b01 }, { store, 1 },
                     { load, 1 }, { getr, 0 }, { geti, 0 }, { print },
                     { load, 1 }, { geti, 1 }, { print } };
    maps = stack_maps( nest, 2 );
    assert(( *maps == stack_maps_t{ { 1, { {}, {} } }, { 5, { { 0 }, { 0 } } } } ));
    r = execute( nest, 2, *maps, 100, true );
    assert(( r.output == std::vector< long >{ 1, 5 } ));

    /* a conflict which is never used is fine – and not a root */
    program conflict = { { push, 1 }, { jz, 5 },
                         { null }, { store, 0 }, { jump, 7 },
                         /* 5 */ { push, 7 }, { store, 0 },
                         /* 7 */ { push, 3 }, { alloc, 1, 0 }, { geti, 0 }, { print } };
    maps = stack_maps( conflict, 1 );
    assert(( *maps == stack_maps_t{ { 8, { {}, {} } } } ));
    assert(( execute( conflict, 1, *maps, 100, true ).output == std::vector< long >{ 3 } ));

    /* … but using it is not */
    program use = conflict;
    use.insert( use.end(), { { load, 0 }, { jnull, 13 } } );
    assert( !stack_maps( use, 1 ) );

    /* unreachable code is not analysed */
    program dead = { { jump, 3 }, { push, 1 }, { alloc, 1, 0 } };
    assert( stack_maps( dead, 0 ) == stack_maps_t{} );

    /* invalid programs */
    assert( !stack_maps( { { null }, { push, 1 }, { add } }, 0 ) );            /* type error */
    assert( !stack_maps( { { pop } }, 0 ) );                                   /* underflow */
    assert( !stack_maps( { { push, 1 }, { jump, 5 } }, 0 ) );                  /* bad jump */
    assert( !stack_maps( { { load, 1 } }, 1 ) );                               /* bad slot */
    assert( !stack_maps( { { push, 1 }, { alloc, 1, 1 } }, 0 ) );              /* mask */
    assert( !stack_maps( { { push, 0 }, { geti, 0 } }, 0 ) );                  /* not a reference */
    assert( stack_maps( { { push, 1 }, { jz, 4 }, { push, 2 }, { push, 3 } }, 0 ) );   /* the end is not an instruction */
    assert( !stack_maps( { { push, 1 }, { jz, 3 }, { push, 2 }, { push, 1 }, { jz, 0 } }, 0 ) );   /* heights */

    return 0;
}
