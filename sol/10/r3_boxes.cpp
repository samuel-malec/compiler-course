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

/* The programs in this exercise are written as S-expressions, so
 * that we can concentrate on evaluation instead of parsing. The
 * function ‹read› (provided) turns the text into a tree of ‹sexp›
 * nodes: each node is either an «atom» – a number, a name, a string
 * literal «including» its quotes (no escapes), or anything else
 * without spaces and parentheses – or a «list» of nodes. ‹offset›
 * is the position of the first character of the node in the input.
 * The function ‹write› does the opposite of ‹read›. If the input is
 * malformed, ‹read› throws ‹std::invalid_argument›. */

struct sexp
{
    std::string atom;           /* non-empty for atoms */
    std::vector< sexp > items;  /* elements of a list */
    std::size_t offset = 0;

    bool is_atom() const { return !atom.empty(); }
    bool is_list() const { return atom.empty(); }
};

void skip_space( std::string_view s, std::size_t &pos )
{
    while ( pos < s.size() && ( s[ pos ] == ' ' || s[ pos ] == '\n' ) )
        ++ pos;
}

sexp read_at( std::string_view s, std::size_t &pos )
{
    skip_space( s, pos );
    sexp out{ "", {}, pos };

    if ( pos >= s.size() || s[ pos ] == ')' )
        throw std::invalid_argument( "unexpected end of list or input" );

    if ( s[ pos ] == '(' )
    {
        for ( ++ pos; skip_space( s, pos ), pos >= s.size() || s[ pos ] != ')'; )
            out.items.push_back( read_at( s, pos ) );
        ++ pos;
        return out;
    }

    std::size_t start = pos;

    if ( s[ pos ] == '"' )
    {
        pos = s.find( '"', pos + 1 );
        if ( pos == s.npos )
            throw std::invalid_argument( "unterminated string" );
        ++ pos;
    }
    else
        while ( pos < s.size() && std::string_view( " \n()" ).find( s[ pos ] ) == s.npos )
            ++ pos;

    out.atom = s.substr( start, pos - start );
    return out;
}

sexp read( std::string_view s )
{
    std::size_t pos = 0;
    sexp e = read_at( s, pos );
    skip_space( s, pos );
    if ( pos != s.size() )
        throw std::invalid_argument( "trailing input" );
    return e;
}

std::string write( const sexp &e )
{
    if ( e.is_atom() )
        return e.atom;

    std::string out;
    for ( const auto &i : e.items )
        out += ( out.empty() ? "" : " " ) + write( i );
    return "(" + out + ")";
}

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

/* Flat closures capture «values». That is fine as long as variables
 * never change – but with assignment, a closure and its creator (or two
 * closures) which share a variable must see each other's changes.
 * Compilers solve this by «assignment conversion»: a variable which is
 * both «captured» (used inside a nested ‹fn›) and «assigned» (the target
 * of a ‹set› anywhere in its scope) is stored in a «box», and the box is
 * what gets captured.
 *
 * Extend the closure compiler of ‹d3_closures› (integers, variables,
 * operators, ‹if›, ‹let›, ‹fn›, calls) with:
 *
 *  • ‹(set x e)› – assign the value of ‹e› to ‹x›; the value of the
 *    expression is the assigned value,
 *  • ‹(do e₁ … eₙ)› – evaluate in order, the value of ‹eₙ› (‹n ≥ 1›),
 *  • ‹(print e)› – print the value of ‹e› and return it.
 *
 * A variable (a parameter or a ‹let›) is boxed if and only if it is
 * both captured and assigned. A boxed variable gets its box when it is
 * bound (‹box› right after the value is computed, or at the start of
 * the function for a parameter); reading it uses ‹unbox›, and ‹set› uses
 * ‹setbox›. Other variables use plain slots (and ‹set› on them is a
 * ‹store›). The main expression's value is not printed (only ‹print›
 * prints). You can assume that the program is well formed. */

// module compile( std::string_view program );

bool is_number( const std::string &a ) { return a.find_first_not_of( "-0123456789" ) == a.npos; }

const std::map< std::string, op > binops =
{
    { "+", op::add }, { "-", op::sub }, { "*", op::mul }, { "/", op::div }, { "%", op::mod },
    { "<", op::lt }, { "<=", op::le }, { "=", op::eq }, { "!=", op::ne },
};

const std::set< std::string > keywords = { "if", "let", "fn", "set", "do", "print" };

std::set< std::string > free_vars( const sexp &e )
{
    if ( e.is_atom() )
        return is_number( e.atom ) ? std::set< std::string >{} : std::set< std::string >{ e.atom };
    const std::string &h = e.items[ 0 ].atom;
    std::set< std::string > out;
    if ( h == "fn" )
    {
        out = free_vars( e.items[ 2 ] );
        for ( const auto &p : e.items[ 1 ].items ) out.erase( p.atom );
    }
    else if ( h == "let" )
    {
        out = free_vars( e.items[ 3 ] );
        out.erase( e.items[ 1 ].atom );
        out.merge( free_vars( e.items[ 2 ] ) );
    }
    else
        for ( std::size_t i = binops.contains( h ) || keywords.contains( h ) ? 1 : 0; i < e.items.size(); ++ i )
            out.merge( free_vars( e.items[ i ] ) );
    return out;
}

/* Is the variable ‹x›, bound just outside of ‹e›, assigned / captured
 * within ‹e›? (Inner bindings of the same name shadow it.) */

void analyse( const sexp &e, const std::string &x, bool inside_fn, bool &assigned, bool &captured )
{
    if ( e.is_atom() )
    {
        if ( e.atom == x && inside_fn ) captured = true;
        return;
    }
    const std::string &h = e.items[ 0 ].atom;
    if ( h == "fn" )
    {
        for ( const auto &p : e.items[ 1 ].items ) if ( p.atom == x ) return;
        return analyse( e.items[ 2 ], x, true, assigned, captured );
    }
    if ( h == "let" )
    {
        analyse( e.items[ 2 ], x, inside_fn, assigned, captured );
        if ( e.items[ 1 ].atom != x ) analyse( e.items[ 3 ], x, inside_fn, assigned, captured );
        return;
    }
    if ( h == "set" )
    {
        if ( e.items[ 1 ].atom == x )
        {
            assigned = true;
            if ( inside_fn ) captured = true;
        }
        return analyse( e.items[ 2 ], x, inside_fn, assigned, captured );
    }
    for ( std::size_t i = binops.contains( h ) || keywords.contains( h ) ? 1 : 0; i < e.items.size(); ++ i )
        analyse( e.items[ i ], x, inside_fn, assigned, captured );
}

bool needs_box( const sexp &scope_body, const std::string &x )
{
    bool assigned = false, captured = false;
    analyse( scope_body, x, false, assigned, captured );
    return assigned && captured;
}

struct scope
{
    std::map< std::string, long > slots;
    std::vector< std::string > captured;
    std::set< std::string > boxed;
    long next = 0;
    int fn;
};

struct compiler
{
    module m;

    void emit( scope &s, op o, long a = 0, long b = 0 ) { m[ s.fn ].code.push_back( { o, a, b } ); }

    void raw( scope &s, const std::string &x )
    {
        if ( auto it = s.slots.find( x ); it != s.slots.end() ) return emit( s, op::load, it->second );
        auto c = std::find( s.captured.begin(), s.captured.end(), x );
        emit( s, op::env, c - s.captured.begin() );
    }

    void expr( scope &s, const sexp &e )
    {
        if ( e.is_atom() )
        {
            if ( is_number( e.atom ) ) return emit( s, op::push, std::stol( e.atom ) );
            raw( s, e.atom );
            if ( s.boxed.contains( e.atom ) ) emit( s, op::unbox );
            return;
        }

        const std::string &h = e.items[ 0 ].atom;

        if ( auto o = binops.find( h ); o != binops.end() )
        {
            expr( s, e.items[ 1 ] );
            expr( s, e.items[ 2 ] );
            return emit( s, o->second );
        }
        if ( h == "if" )
        {
            expr( s, e.items[ 1 ] );
            std::size_t jz = m[ s.fn ].code.size();
            emit( s, op::jz );
            expr( s, e.items[ 2 ] );
            std::size_t jump = m[ s.fn ].code.size();
            emit( s, op::jump );
            m[ s.fn ].code[ jz ].arg = m[ s.fn ].code.size();
            expr( s, e.items[ 3 ] );
            m[ s.fn ].code[ jump ].arg = m[ s.fn ].code.size();
            return;
        }
        if ( h == "do" )
        {
            for ( std::size_t i = 1; i < e.items.size(); ++ i )
            {
                expr( s, e.items[ i ] );
                if ( i + 1 < e.items.size() ) emit( s, op::pop );
            }
            return;
        }
        if ( h == "print" )
        {
            expr( s, e.items[ 1 ] );
            emit( s, op::dup );
            return emit( s, op::print );
        }
        if ( h == "set" )
        {
            const std::string &x = e.items[ 1 ].atom;
            if ( s.boxed.contains( x ) )
            {
                raw( s, x );
                expr( s, e.items[ 2 ] );
                return emit( s, op::setbox );
            }
            expr( s, e.items[ 2 ] );
            emit( s, op::dup );
            return emit( s, op::store, s.slots.at( x ) );
        }
        if ( h == "let" )
        {
            const std::string &x = e.items[ 1 ].atom;
            expr( s, e.items[ 2 ] );
            bool box = needs_box( e.items[ 3 ], x );
            if ( box ) emit( s, op::box );
            long slot = s.next++;
            m[ s.fn ].slots = std::max( m[ s.fn ].slots, int( s.next ) );
            emit( s, op::store, slot );
            auto saved = s.slots;
            auto saved_boxed = s.boxed;
            s.slots[ x ] = slot;
            if ( box ) s.boxed.insert( x ); else s.boxed.erase( x );
            expr( s, e.items[ 3 ] );
            s.slots = saved, s.boxed = saved_boxed;
            -- s.next;
            return;
        }
        if ( h == "fn" )
        {
            int k = m.size();
            m.push_back( { "lambda" } );
            scope inner{ {}, {}, {}, 0, k };
            for ( const auto &p : e.items[ 1 ].items ) inner.slots[ p.atom ] = inner.next++;
            m[ k ].params = m[ k ].slots = inner.next;
            for ( const auto &v : free_vars( e ) )
            {
                inner.captured.push_back( v );
                if ( s.boxed.contains( v ) ) inner.boxed.insert( v );
            }
            for ( const auto &p : e.items[ 1 ].items )   /* box the parameters which need it */
                if ( needs_box( e.items[ 2 ], p.atom ) )
                {
                    emit( inner, op::load, inner.slots[ p.atom ] );
                    emit( inner, op::box );
                    emit( inner, op::store, inner.slots[ p.atom ] );
                    inner.boxed.insert( p.atom );
                }
            expr( inner, e.items[ 2 ] );
            emit( inner, op::ret );

            for ( const auto &v : inner.captured ) raw( s, v );
            return emit( s, op::closure, k, inner.captured.size() );
        }

        expr( s, e.items[ 0 ] );
        for ( std::size_t i = 1; i < e.items.size(); ++ i ) expr( s, e.items[ i ] );
        emit( s, op::callv, e.items.size() - 1 );
    }
};

module compile( std::string_view program )
{
    compiler c;
    c.m.push_back( { "main" } );
    scope s{ {}, {}, {}, 0, 0 };
    c.expr( s, read( program ) );
    c.emit( s, op::pop );
    return c.m;
}

int main()
{
    using v = std::vector< long >;
    auto run = []( std::string_view p ) { outcome o = execute( compile( p ) ); assert( o.ok ); return o.output; };
    auto boxes = []( std::string_view p )
    {
        int n = 0;
        for ( const auto &f : compile( p ) )
            for ( const auto &i : f.code ) n += i.o == op::box;
        return n;
    };

    std::string counter = "(let make (fn () (let n 0 (fn () (set n (+ n 1)))))"
                          " (let a (make) (let b (make) (do (a) (a) (print (a)) (print (b))))))";
    assert(( run( counter ) == v{ 3, 1 } ));
    assert( boxes( counter ) == 1 );

    /* the creator sees changes made by the closure, and vice versa */
    assert(( run( "(let x 1 (let inc (fn () (set x (+ x 1))) (do (inc) (set x (* x 10)) (inc) (print x))))" ) ==
             v{ 21 } ));

    /* two closures sharing a parameter */
    assert(( run( "(let acct (fn (balance) (fn (op amount) (if (= op 0) balance (set balance (- balance amount)))))"
                  " (let a (acct 100) (do (a 1 30) (a 1 20) (print (a 0 0)))))" ) == v{ 50 } ));

    /* assigned but not captured, and captured but not assigned: no boxes */
    std::string plain = "(let x 1 (let f (fn (y) (+ x y)) (let z 5 (do (set z (f z)) (print z)))))";
    assert(( run( plain ) == v{ 6 } ));
    assert( boxes( plain ) == 0 );

    /* each call gets a fresh box */
    assert(( run( "(let mk (fn (s) (fn () (set s (* s 2))))"
                  " (let a (mk 1) (let b (mk 10) (do (a) (a) (print (a)) (print (b))))))" ) == v{ 8, 20 } ));

    return 0;
}
