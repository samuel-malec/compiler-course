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

/* A run-time error is much easier to understand with a stack trace
 * which points to the source: ‹division by zero at line 3 in f, called
 * from line 7 in main›. The VM reports the active frames as pairs of a
 * function and an instruction index; to translate those to source
 * lines, a compiler emits a «line table». Write a compiler for the
 * language of ‹e3_return› (functions with statement bodies), extended
 * with ‹(while c s)› and ‹(do e)› (evaluate and drop), which fills in the
 * ‹lines› of every function: ‹lines[ i ]› is the line (counted from 1) of
 * the «innermost statement» which instruction ‹i› belongs to. The
 * instructions which return 0 at the end of a function belong to the
 * line of the ‹(fun› of that function. Everything else is as in
 * ‹e3_return› (‹main› is function 0).
 *
 * The tests use the line table to format a stack trace ‹NAME:LINE›,
 * innermost first. You can assume that the program is well formed. */

// module compile( std::string_view program );

struct compiler
{
    std::map< std::string, int > index = {};
    module m = {};
    function *fn = nullptr;
    std::vector< std::map< std::string, long > > scopes = {};
    long next = 0;
    int line = 1;                 /* the line of the current statement */
    std::string_view src = {};

    int line_of( const sexp &s ) const
    {
        return 1 + std::count( src.begin(), src.begin() + s.offset, '\n' );
    }

    void emit( op o, long arg = 0 )
    {
        fn->code.push_back( { o, arg } );
        fn->lines.push_back( line );
    }

    long lookup( const std::string &x )
    {
        for ( auto it = scopes.rbegin(); it != scopes.rend(); ++ it )
            if ( auto s = it->find( x ); s != it->end() ) return s->second;
        throw std::logic_error( "unbound" );
    }

    void expr( const sexp &e )
    {
        if ( e.is_atom() )
        {
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos ) emit( op::push, std::stol( e.atom ) );
            else emit( op::load, lookup( e.atom ) );
            return;
        }
        const std::string &h = e.items[ 0 ].atom;
        for ( std::size_t i = 1; i < e.items.size(); ++ i ) expr( e.items[ i ] );
        static const std::map< std::string, op > ops =
        {
            { "+", op::add }, { "-", op::sub }, { "*", op::mul }, { "/", op::div }, { "%", op::mod },
            { "<", op::lt }, { "<=", op::le }, { "=", op::eq }, { "!=", op::ne },
        };
        if ( h == ">" || h == ">=" ) { emit( op::swap ); emit( h == ">" ? op::lt : op::le ); }
        else if ( auto o = ops.find( h ); o != ops.end() ) emit( o->second );
        else emit( op::call, index.at( h ) );
    }

    void scoped( const sexp &s )
    {
        scopes.emplace_back();
        long saved = next;
        stmt( s );
        next = saved;
        scopes.pop_back();
    }

    void stmt( const sexp &s )
    {
        int saved_line = line;
        line = line_of( s );
        const std::string &h = s.items[ 0 ].atom;

        if ( h == "block" )
        {
            scopes.emplace_back();
            long saved = next;
            for ( std::size_t i = 1; i < s.items.size(); ++ i ) stmt( s.items[ i ] );
            next = saved;
            scopes.pop_back();
        }
        else if ( h == "var" )
        {
            expr( s.items[ 2 ] );
            long slot = next++;
            fn->slots = std::max( fn->slots, int( next ) );
            scopes.back()[ s.items[ 1 ].atom ] = slot;
            emit( op::store, slot );
        }
        else if ( h == "set" ) { expr( s.items[ 2 ] ); emit( op::store, lookup( s.items[ 1 ].atom ) ); }
        else if ( h == "print" ) { expr( s.items[ 1 ] ); emit( op::print ); }
        else if ( h == "do" ) { expr( s.items[ 1 ] ); emit( op::pop ); }
        else if ( h == "return" ) { expr( s.items[ 1 ] ); emit( op::ret ); }
        else if ( h == "while" )
        {
            std::size_t top = fn->code.size();
            expr( s.items[ 1 ] );
            std::size_t jz = fn->code.size();
            emit( op::jz );
            scoped( s.items[ 2 ] );
            line = line_of( s );
            emit( op::jump, top );
            fn->code[ jz ].arg = fn->code.size();
        }
        else /* if */
        {
            expr( s.items[ 1 ] );
            std::size_t jz = fn->code.size();
            emit( op::jz );
            scoped( s.items[ 2 ] );
            line = line_of( s );
            if ( s.items.size() == 4 )
            {
                std::size_t jump = fn->code.size();
                emit( op::jump );
                fn->code[ jz ].arg = fn->code.size();
                scoped( s.items[ 3 ] );
                fn->code[ jump ].arg = fn->code.size();
            }
            else
                fn->code[ jz ].arg = fn->code.size();
        }

        line = saved_line;
    }
};

module compile( std::string_view program )
{
    sexp p = read( program );
    compiler c;
    c.src = program;
    c.m.resize( p.items.size() - 1 );

    std::vector< const sexp * > order;
    for ( std::size_t i = 1; i < p.items.size(); ++ i )
        if ( p.items[ i ].items[ 1 ].atom == "main" ) order.insert( order.begin(), &p.items[ i ] );
        else order.push_back( &p.items[ i ] );

    for ( std::size_t i = 0; i < order.size(); ++ i )
    {
        c.index[ order[ i ]->items[ 1 ].atom ] = i;
        c.m[ i ].name = order[ i ]->items[ 1 ].atom;
        c.m[ i ].params = c.m[ i ].slots = order[ i ]->items[ 2 ].items.size();
    }

    for ( std::size_t i = 0; i < order.size(); ++ i )
    {
        const sexp &f = *order[ i ];
        c.fn = &c.m[ i ];
        c.scopes = { {} };
        c.next = 0;
        for ( const auto &x : f.items[ 2 ].items ) c.scopes.back()[ x.atom ] = c.next++;
        c.scoped( f.items[ 3 ] );
        c.line = c.line_of( f );
        c.emit( op::push, 0 );
        c.emit( op::ret );
    }

    return c.m;
}

int main()
{
    auto trace = []( std::string_view p )
    {
        module m = compile( p );
        for ( const auto &f : m )
            assert( f.lines.size() == f.code.size() );
        outcome o = execute( m );
        std::vector< std::string > out;
        for ( auto [ fn, pc ] : o.trace )
            out.push_back( m[ fn ].name + ":" + std::to_string( m[ fn ].lines[ pc ] ) );
        return out;
    };
    using v = std::vector< std::string >;

    std::string prog =
        "(program\n"
        " (fun inv (x)\n"
        "   (block\n"
        "     (var y (- x 1))\n"
        "     (return (/ 100 y))))\n"
        " (fun main ()\n"
        "   (block\n"
        "     (print (inv 3))\n"
        "     (var i 3)\n"
        "     (while (> i 0)\n"
        "       (block (set i (- i 1))\n"
        "         (do (inv i)))))))";
    assert(( trace( prog ) == v{ "inv:5", "main:12" } ));

    assert(( trace( "(program (fun main ()\n"
                    "  (block\n"
                    "    (print 1)\n"
                    "    (print (f 0)))) (fun f (a)\n"
                    "  (if (= a 0)\n"
                    "    (print (/ 1 a)))))" ) == v{ "f:6", "main:4" } ));

    module m = compile( "(program (fun main ()\n (block (do (f))))\n(fun f ()\n (block)))" );
    assert( m[ 1 ].lines.back() == 3 );   /* the implicit ‹return 0› */
    assert( m[ 0 ].lines[ 0 ] == 2 );

    return 0;
}
