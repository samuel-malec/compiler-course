#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* The virtual machine of this chapter (see ‹d2_calls›). It extends the
 * machine of Chapter 9 with «functions». A «module» is a list of
 * functions; function 0 is the entry point (‹main›), which takes no
 * parameters. Each function has its own code (jump targets are indices
 * into it), a number of parameters, a number of local slots
 * (including the parameters, which occupy slots ‹0 … params - 1›), a
 * table of exception handlers and, optionally, a line table.
 *
 * Every call creates a «frame» with the function's slots and its own
 * operand stack. The instructions of Chapter 9 work within the current
 * frame; in addition:
 *
 *  • ‹call k› – pop as many values as function ‹k› has parameters (the
 *    last argument is on top), and call it with them,
 *  • ‹tailcall k› – the same, but the new frame «replaces» the current
 *    one (the callee returns directly to our caller),
 *  • ‹ret› – pop the return value, discard the frame and push the value
 *    onto the caller's stack; ‹ret› in ‹main› (or ‹halt›, or running off
 *    the end of ‹main›) ends the program; running off the end of any
 *    other function is an error,
 *  • ‹closure k n› – pop ‹n› values and push a «closure» of function ‹k›
 *    which captures them (the first popped value is captured last),
 *  • ‹callv n›, ‹tailcallv n› – pop ‹n› arguments, then a closure, and call
 *    its function (which must have ‹n› parameters) with them,
 *  • ‹env i› – push the ‹i›-th value captured by the closure which is
 *    running (an error in a function called directly by ‹call›),
 *  • ‹box› – pop a value, push a new «box» (a mutable cell) holding it;
 *    ‹unbox› – pop a box, push its content; ‹setbox› – pop a value, pop
 *    a box, store the value in the box and push the value again,
 *  • ‹throw› – pop a value and «throw» it: the handlers of the current
 *    function are searched (in order) for one whose range ‹[from, to)›
 *    contains the throwing instruction; if found, the operand stack is
 *    cleared, the value is pushed and execution continues at the
 *    handler's ‹target›; otherwise, the frame is discarded and the
 *    search continues in the caller, at the ‹call› instruction which
 *    created the frame; an exception which leaves ‹main› ends the
 *    program and is reported as ‹uncaught›.
 *
 * Values are integers, closures and boxes; arithmetic, comparisons,
 * ‹jz› and ‹print› require integers. ‹execute› runs a module; on a
 * run-time error, the outcome has ‹ok == false› and ‹trace› lists the
 * active frames as (function, index of the current instruction),
 * innermost first. ‹max_frames› is the largest number of frames which
 * existed at the same time. */

enum class op
{
    push, load, store, add, sub, mul, div, mod, lt, le, eq, ne, neg, not_,
    dup, pop, swap, jump, jz, print, halt,
    call, tailcall, ret, closure, callv, tailcallv, env, box, unbox, setbox, throw_
};

struct instr
{
    op o;
    long arg = 0, arg2 = 0;
    bool operator==( const instr & ) const = default;
};

using program = std::vector< instr >;

struct handler { std::size_t from, to, target; };

struct function
{
    std::string name;
    int params = 0, slots = 0;
    program code = {};
    std::vector< handler > handlers = {};
    std::vector< int > lines = {};   /* source line of each instruction (optional) */
};

using module = std::vector< function >;

struct outcome
{
    std::vector< long > output;
    bool ok = true;
    std::optional< long > uncaught = std::nullopt;
    std::vector< std::pair< int, std::size_t > > trace = {};
    std::size_t max_frames = 0;
    long steps = 0;
};

const char *op_names[] = { "push", "load", "store", "add", "sub", "mul", "div", "mod", "lt", "le",
                           "eq", "ne", "neg", "not", "dup", "pop", "swap", "jump", "jz", "print",
                           "halt", "call", "tailcall", "ret", "closure", "callv", "tailcallv", "env",
                           "box", "unbox", "setbox", "throw" };

std::string show( const instr &i )
{
    std::string s = op_names[ int( i.o ) ];
    switch ( i.o )
    {
        case op::push: case op::load: case op::store: case op::jump: case op::jz: case op::call:
        case op::tailcall: case op::callv: case op::tailcallv: case op::env:
            return s + " " + std::to_string( i.arg );
        case op::closure:
            return s + " " + std::to_string( i.arg ) + " " + std::to_string( i.arg2 );
        default:
            return s;
    }
}

/* A value: an integer, a closure or a box. Closures and boxes live in
 * an «arena» owned by ‹execute› and are all freed when it returns – the
 * VM has no garbage collector (that is the topic of Chapter 12), and
 * reference counting would leak cycles (a recursive closure in a box). */

struct vm_value;
using vm_closure = std::pair< int, std::vector< vm_value > >;

struct vm_value
{
    long n = 0;
    const vm_closure *fn = nullptr;
    vm_value *box = nullptr;
};

outcome execute( const module &m, long fuel = 1000000 )
{
    struct frame
    {
        int fn;
        std::size_t pc = 0;
        std::vector< vm_value > slots, stack = {};
        const vm_closure *self = nullptr;
    };

    outcome res;
    std::vector< std::unique_ptr< vm_closure > > closures;
    std::vector< std::unique_ptr< vm_value > > boxes;
    std::vector< frame > frames;
    struct vm_error {};

    auto enter = [ & ]( int k, std::vector< vm_value > args, const vm_closure *self, bool tail )
    {
        if ( k < 0 || std::size_t( k ) >= m.size() || int( args.size() ) != m[ k ].params ||
             m[ k ].slots < m[ k ].params )
            throw vm_error();
        args.resize( m[ k ].slots );
        if ( tail ) frames.pop_back();
        frames.push_back( { k, 0, std::move( args ), {}, self } );
        res.max_frames = std::max( res.max_frames, frames.size() );
    };

    try
    {
        if ( m.empty() ) return res;
        enter( 0, {}, nullptr, false );

        while ( true )
        {
            frame &f = frames.back();
            const program &code = m[ f.fn ].code;

            if ( f.pc >= code.size() )
            {
                if ( f.fn != 0 || frames.size() != 1 ) throw vm_error();
                return res;
            }
            if ( ++ res.steps > fuel ) throw vm_error();

            const instr &i = code[ f.pc++ ];
            auto &st = f.stack;
            auto pop = [ & ]() -> vm_value
            {
                if ( st.empty() ) throw vm_error();
                vm_value v = st.back();
                st.pop_back();
                return v;
            };
            auto pop_int = [ & ]() -> long
            {
                vm_value v = pop();
                if ( v.fn || v.box ) throw vm_error();
                return v.n;
            };
            auto slot = [ & ]() -> vm_value &
            {
                if ( i.arg < 0 || i.arg >= long( f.slots.size() ) ) throw vm_error();
                return f.slots[ i.arg ];
            };
            auto jump = [ & ]
            {
                if ( i.arg < 0 || std::size_t( i.arg ) > code.size() ) throw vm_error();
                f.pc = i.arg;
            };
            auto args = [ & ]( long n )
            {
                if ( n < 0 || std::size_t( n ) > st.size() ) throw vm_error();
                std::vector< vm_value > a( st.end() - n, st.end() );
                st.resize( st.size() - n );
                return a;
            };

            switch ( i.o )
            {
                case op::push: st.push_back( { i.arg } ); break;
                case op::load: st.push_back( slot() ); break;
                case op::store: { vm_value v = pop(); slot() = v; break; }
                case op::neg: st.push_back( { -pop_int() } ); break;
                case op::not_: st.push_back( { pop_int() == 0 } ); break;
                case op::dup: { vm_value v = pop(); st.push_back( v ), st.push_back( v ); break; }
                case op::pop: pop(); break;
                case op::swap: { vm_value b = pop(), a = pop(); st.push_back( b ), st.push_back( a ); break; }
                case op::jump: jump(); break;
                case op::jz: { long v = pop_int(); if ( v == 0 ) jump(); break; }
                case op::print: res.output.push_back( pop_int() ); break;
                case op::halt: return res;

                case op::call: case op::tailcall:
                {
                    if ( i.arg < 0 || std::size_t( i.arg ) >= m.size() ) throw vm_error();
                    auto a = args( m[ i.arg ].params );
                    enter( i.arg, std::move( a ), nullptr, i.o == op::tailcall );
                    break;
                }
                case op::callv: case op::tailcallv:
                {
                    auto a = args( i.arg );
                    vm_value c = pop();
                    if ( !c.fn ) throw vm_error();
                    enter( c.fn->first, std::move( a ), c.fn, i.o == op::tailcallv );
                    break;
                }
                case op::ret:
                {
                    vm_value v = pop();
                    if ( frames.size() == 1 ) return res;
                    frames.pop_back();
                    frames.back().stack.push_back( v );
                    break;
                }
                case op::closure:
                {
                    auto captured = args( i.arg2 );
                    if ( i.arg < 0 || std::size_t( i.arg ) >= m.size() ) throw vm_error();
                    closures.push_back( std::make_unique< vm_closure >( i.arg, std::move( captured ) ) );
                    st.push_back( { 0, closures.back().get() } );
                    break;
                }
                case op::env:
                    if ( !f.self || i.arg < 0 || std::size_t( i.arg ) >= f.self->second.size() ) throw vm_error();
                    st.push_back( f.self->second[ i.arg ] );
                    break;
                case op::box:
                {
                    vm_value v = pop();
                    boxes.push_back( std::make_unique< vm_value >( v ) );
                    st.push_back( { 0, nullptr, boxes.back().get() } );
                    break;
                }
                case op::unbox: { vm_value b = pop(); if ( !b.box ) throw vm_error(); st.push_back( *b.box ); break; }
                case op::setbox:
                {
                    vm_value v = pop(), b = pop();
                    if ( !b.box ) throw vm_error();
                    *b.box = v;
                    st.push_back( v );
                    break;
                }
                case op::throw_:
                {
                    long v = pop_int();
                    while ( true )
                    {
                        frame &g = frames.back();
                        std::size_t at = g.pc - 1;
                        const handler *h = nullptr;
                        for ( const auto &x : m[ g.fn ].handlers )
                            if ( x.from <= at && at < x.to ) { h = &x; break; }
                        if ( h )
                        {
                            g.stack.clear();
                            g.stack.push_back( { v } );
                            g.pc = h->target;
                            break;
                        }
                        if ( frames.size() == 1 ) { res.uncaught = v; res.ok = false; return res; }
                        frames.pop_back();
                    }
                    break;
                }
                default:
                {
                    long b = pop_int(), a = pop_int();
                    if ( ( i.o == op::div || i.o == op::mod ) && b == 0 ) throw vm_error();
                    long r = i.o == op::add ? a + b : i.o == op::sub ? a - b : i.o == op::mul ? a * b :
                             i.o == op::div ? a / b : i.o == op::mod ? a % b : i.o == op::lt ? a < b :
                             i.o == op::le ? a <= b : i.o == op::eq ? a == b : a != b;
                    st.push_back( { r } );
                }
            }
        }
    }
    catch ( const vm_error & )
    {
        res.ok = false;
        for ( auto it = frames.rbegin(); it != frames.rend(); ++ it )
            res.trace.emplace_back( it->fn, it->pc ? it->pc - 1 : 0 );
        return res;
    }
}

/* Real compilers translate each source file separately into an
 * «object file», and a «linker» combines the object files into an
 * executable, connecting the uses of functions with their definitions
 * («symbol resolution») and fixing up the references («relocation»).
 * In this exercise, an object is: */

struct object
{
    std::vector< function > functions;                    /* defined here */
    std::vector< std::pair< std::string, int > > imports; /* used here: name and arity */
};

/* In the code of an object, the function operand ‹k› of ‹call›,
 * ‹tailcall› and ‹closure› refers to the object's own function ‹k› if
 * ‹k ≥ 0›, and to import number ‹-k - 1› if ‹k < 0›. Write a function
 * ‹link_objects› which produces a module in which:
 *
 *  • the function called ‹main› is function 0, and all the other
 *    functions follow in the order of the objects, and within each
 *    object, in order,
 *  • every function operand refers to the right function of the module.
 *
 * Return ‹std::nullopt› if there is no ‹main› or it has parameters, if
 * two functions (in any objects) have the same name, if an import does
 * not match any function or matches one with a different number of
 * parameters, or if a function operand is out of range. */

// std::optional< module > link_objects( const std::vector< object > &objects );

std::optional< module > link_objects( const std::vector< object > &objects )
{
    /* assign the final indices: main first, then the rest in order */
    std::map< std::string, int > index;
    std::vector< std::pair< std::size_t, std::size_t > > origin; /* final index → (object, function) */
    std::optional< std::pair< std::size_t, std::size_t > > main;

    for ( std::size_t o = 0; o < objects.size(); ++ o )
        for ( std::size_t f = 0; f < objects[ o ].functions.size(); ++ f )
        {
            const auto &fn = objects[ o ].functions[ f ];
            if ( index.contains( fn.name ) ) return std::nullopt;
            index[ fn.name ] = -1;
            if ( fn.name == "main" ) main = { o, f };
            else origin.emplace_back( o, f );
        }

    if ( !main || objects[ main->first ].functions[ main->second ].params != 0 ) return std::nullopt;
    origin.insert( origin.begin(), *main );
    for ( std::size_t k = 0; k < origin.size(); ++ k )
        index[ objects[ origin[ k ].first ].functions[ origin[ k ].second ].name ] = k;

    /* for each object: own function number → final index, import → final index */
    std::vector< std::vector< int > > own( objects.size() ), imported( objects.size() );
    for ( std::size_t k = 0; k < origin.size(); ++ k )
    {
        auto [ o, f ] = origin[ k ];
        own[ o ].resize( objects[ o ].functions.size() );
        own[ o ][ f ] = k;
    }
    for ( std::size_t o = 0; o < objects.size(); ++ o )
        for ( const auto &[ name, arity ] : objects[ o ].imports )
        {
            auto it = index.find( name );
            if ( it == index.end() ) return std::nullopt;
            auto [ oo, ff ] = origin[ it->second ];
            if ( objects[ oo ].functions[ ff ].params != arity ) return std::nullopt;
            imported[ o ].push_back( it->second );
        }

    module m;
    for ( auto [ o, f ] : origin )
    {
        function fn = objects[ o ].functions[ f ];
        for ( auto &i : fn.code )
            if ( i.o == op::call || i.o == op::tailcall || i.o == op::closure )
            {
                long k = i.arg;
                if ( k >= 0 )
                {
                    if ( std::size_t( k ) >= own[ o ].size() ) return std::nullopt;
                    i.arg = own[ o ][ k ];
                }
                else
                {
                    if ( std::size_t( -k - 1 ) >= imported[ o ].size() ) return std::nullopt;
                    i.arg = imported[ o ][ -k - 1 ];
                }
            }
        m.push_back( fn );
    }
    return m;
}

int main()
{
    using v = std::vector< long >;

    /* lib: square( x ) = x * x;  twice( x ) = 2 * x */
    object lib{ { { "square", 1, 1, { { op::load, 0 }, { op::load, 0 }, { op::mul }, { op::ret } } },
                  { "twice", 1, 1, { { op::push, 2 }, { op::load, 0 }, { op::mul }, { op::ret } } } },
                {} };

    /* app: main prints quad( 3 ) and helper( 5 ); quad( x ) = square( twice( x ) ) */
    object app{ { { "main", 0, 0, { { op::push, 3 }, { op::call, 1 }, { op::print },
                                    { op::push, 5 }, { op::call, -3 }, { op::print } } },
                  { "quad", 1, 1, { { op::load, 0 }, { op::call, -2 }, { op::call, -1 }, { op::ret } } } },
                { { "square", 1 }, { "twice", 1 }, { "helper", 1 } } };

    /* more: helper( x ) = quad( x ) + 1, via a closure */
    object more{ { { "helper", 1, 1, { { op::closure, -1, 0 }, { op::load, 0 }, { op::callv, 1 },
                                       { op::push, 1 }, { op::add }, { op::ret } } } },
                 { { "quad", 1 } } };

    auto m = link_objects( { lib, app, more } );
    assert( m );
    assert( m->size() == 5 );
    assert( ( *m )[ 0 ].name == "main" && ( *m )[ 1 ].name == "square" && ( *m )[ 4 ].name == "helper" );
    assert(( execute( *m ).output == v{ 36, 101 } ));

    auto again = link_objects( { more, app, lib } );
    assert( again && ( *again )[ 1 ].name == "helper" );
    assert(( execute( *again ).output == v{ 36, 101 } ));

    object bad_arity{ { { "main", 0, 0, { { op::push, 1 }, { op::push, 2 }, { op::call, -1 }, { op::print } } } },
                      { { "square", 2 } } };
    assert( !link_objects( { lib, bad_arity } ) );
    assert( !link_objects( { app, lib } ) );                 /* ‹helper› is missing */
    assert( !link_objects( { lib } ) );                      /* no ‹main› */
    assert( !link_objects( { lib, lib, app, more } ) );      /* duplicate definitions */
    object out_of_range{ { { "main", 0, 0, { { op::call, 7 } } } }, {} };
    assert( !link_objects( { out_of_range } ) );

    return 0;
}
