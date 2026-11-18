#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* In this assignment, you will write a complete compiler for a small
 * C-like language, from the source text to the bytecode of the
 * virtual machine of Chapter 10 (given below). It brings together
 * everything from Chapters 9 and 10 and the parsing techniques of
 * block 1.
 *
 * ── Syntax ─────────────────────────────────────────────────────────
 *
 *     program = { func } ;
 *     func    = 'fn', ident, '(', [ ident, { ',', ident } ], ')', block ;
 *     block   = '{', { stmt }, '}' ;
 *     stmt    = 'var', ident, '=', expr, ';'
 *             | ident, '=', expr, ';'
 *             | 'if', '(', expr, ')', block, [ 'else', ( block | if ) ]
 *             | 'while', '(', expr, ')', block
 *             | 'return', expr, ';'
 *             | 'break', ';' | 'continue', ';'
 *             | 'print', expr, ';'
 *             | 'throw', expr, ';'
 *             | 'try', block, 'catch', '(', ident, ')', block
 *             | block
 *             | expr, ';' ;
 *
 *     expr    = and, { '||', and } ;
 *     and     = eq, { '&&', eq } ;
 *     eq      = cmp, [ ( '==' | '!=' ), cmp ] ;
 *     cmp     = sum, [ ( '<' | '<=' | '>' | '>=' ), sum ] ;
 *     sum     = term, { ( '+' | '-' ), term } ;
 *     term    = unary, { ( '*' | '/' | '%' ), unary } ;
 *     unary   = ( '-' | '!' ), unary | primary ;
 *     primary = integer | ident | ident, '(', [ expr, { ',', expr } ], ')'
 *             | '(', expr, ')' ;
 *
 * (‹if› in the ‹else› branch is the ‹if› statement.) Identifiers are
 * letters, digits and ‹_›, starting with a letter or ‹_›, other than the
 * keywords. Integers are sequences of digits. Comments start with ‹//›
 * and extend to the end of the line. Lines are numbered from 1.
 *
 * ── Semantics ──────────────────────────────────────────────────────
 *
 * All values are integers. Execution starts in the function ‹main›,
 * which must exist and have no parameters. Functions can call each
 * other in any order (regardless of the order of definitions); a
 * function which ends without ‹return› returns 0.
 *
 *  • Variables are local to the function, lexically scoped: each block
 *    (including the body) opens a scope; ‹var› declares a variable in
 *    the current scope, and may shadow a variable of an enclosing scope,
 *    but not one of the same scope; parameters are in the scope of the
 *    body. A variable is visible from the end of its declaration.
 *  • The operators are those of C (on ‹long›), comparisons and ‹!› give 1
 *    or 0, ‹&&› and ‹||› evaluate the right operand only if needed and
 *    give 1 or 0. Operands are evaluated from left to right, and so are
 *    the arguments of a call. Division by zero is a run-time error (the
 *    VM reports it).
 *  • A condition is true if it is not 0. ‹break› and ‹continue› refer to
 *    the innermost ‹while›.
 *  • ‹throw e› throws the value of ‹e›. If it is thrown (directly, or from
 *    a called function) while the «try block» of a ‹try› statement
 *    runs, execution continues with the catch block, with the value
 *    bound to the catch variable (in the scope of the catch block); the
 *    innermost ‹try› wins. An exception which is not caught ends the
 *    program (the VM reports it as ‹uncaught›).
 *
 * ── Compilation ────────────────────────────────────────────────────
 *
 * Write ‹compile( source )›, which returns a module for the VM below:
 * function 0 is ‹main›, the others follow in the order of their
 * definitions; each function has the name, parameters and code you
 * would expect. Moreover:
 *
 *  • a call which is the operand of ‹return› must be compiled as a
 *    «tail call» (‹tailcall›), so that tail-recursive functions run in
 *    constant stack space,
 *  • ‹try› is compiled with the handler tables of the VM (Chapter 10,
 *    ‹p4_exceptions›),
 *  • every function has a «line table»: the line of each instruction is
 *    the line on which the innermost «statement» it belongs to starts –
 *    e.g. the instructions of a ‹while› condition, and the jump back to
 *    it, have the line of the ‹while› keyword (‹else if› starts a new
 *    statement at the ‹if›); the final ‹return 0› which the compiler adds
 *    has the line of the closing brace of the function.
 *
 * Errors are reported by throwing a ‹compile_error› with the line where
 * the error was detected and one of these messages: ‹syntax error›,
 * ‹undeclared variable X›, ‹redeclared variable X›, ‹unknown function X›,
 * ‹duplicate function X›, ‹wrong number of arguments›, ‹break outside of
 * a loop›, ‹continue outside of a loop›, ‹no main› (line 0). The line of
 * a syntax error is that of the unexpected token (or of the last
 * token, at the end of input); the line of a name error is that of the
 * name; the line of ‹wrong number of arguments› is that of the function
 * name in the call (in a definition of ‹main›: its name). Report the
 * first error in the order of the source text. */

struct compile_error
{
    int line;
    std::string message;
};

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

// module compile( std::string_view source );

int main()
{
    auto run = []( std::string_view src, long fuel = 1000000 ) { return execute( compile( src ), fuel ); };
    using v = std::vector< long >;
    auto error = []( std::string_view src ) -> std::pair< int, std::string >
    {
        try { compile( src ); } catch ( const compile_error &e ) { return { e.line, e.message }; }
        return { -1, "" };
    };

    assert(( run( "fn main() { print 1 + 2 * 3; print -7 / 2; print 7 % -3; print !5 + (2 > 1); }" ).output ==
             v{ 7, -3, 1, 1 } ));

    auto fact = run( "fn main() { print fact(10); }\n"
                     "fn fact(n) { if (n <= 1) { return 1; } return n * fact(n - 1); }" );
    assert( fact.ok && ( fact.output == v{ 3628800 } ) && fact.max_frames == 11 );

    assert(( run( "fn main() {\n"
                  "  var i = 0; var s = 0;\n"
                  "  while (1) {\n"
                  "    i = i + 1;\n"
                  "    if (i % 2 == 0) { continue; } else if (i > 9) { break; }\n"
                  "    s = s + i;\n"
                  "  }\n"
                  "  print s; print i;\n"
                  "}" ).output == v{ 25, 11 } ));

    assert(( run( "fn main() { var x = 1; { var x = 2; print x; x = 3; } print x; }" ).output == v{ 2, 1 } ));
    assert(( run( "fn main() { print 0 && f(); print 1 || f(); print 2 && 3; }\n"
                  "fn f() { print 99; return 1; }" ).output == v{ 0, 1, 1 } ));
    assert(( run( "fn main() { f(1); print g(); }  fn f(x) { print x; }  fn g() { }" ).output == v{ 1, 0 } ));

    /* tail calls */
    auto loop = run( "fn main() { print sum(100000, 0); }\n"
                     "fn sum(n, acc) { if (n == 0) { return acc; } return sum(n - 1, acc + n); }", 10000000 );
    assert( loop.ok && ( loop.output == v{ 5000050000 } ) && loop.max_frames == 2 );

    /* exceptions */
    auto ex = run( "fn main() {\n"
                   "  try { print 1; f(5); print 2; } catch (e) { print e; }\n"
                   "  var i = 0;\n"
                   "  while (i < 3) { try { if (i == 1) { throw 10; } print i; } catch (x) { print x + i; } i = i + 1; }\n"
                   "  throw 42;\n"
                   "}\n"
                   "fn f(x) { try { throw x; } catch (y) { throw y * 2; } }" );
    assert( !ex.ok && ex.uncaught == 42 && ( ex.output == v{ 1, 10, 0, 11, 2 } ) );

    /* line tables */
    auto m = compile( "fn main() {\n"
                      "  print 1;\n"
                      "  print f(0);\n"
                      "}\n"
                      "fn f(x) {\n"
                      "  var y = 1;\n"
                      "  return y / x;\n"
                      "}\n" );
    auto o = execute( m );
    assert( !o.ok && ( o.output == v{ 1 } ) && o.trace.size() == 2 );
    auto line = [ & ]( std::pair< int, std::size_t > at ) { return m[ at.first ].lines[ at.second ]; };
    assert( m[ o.trace[ 0 ].first ].name == "f" && line( o.trace[ 0 ] ) == 7 && line( o.trace[ 1 ] ) == 3 );

    auto w = compile( "fn main() {\n  var x = 0;\n  while (\n    1 / x) { print 1; }\n}" );
    o = execute( w );
    assert( !o.ok && w[ 0 ].lines[ o.trace[ 0 ].second ] == 3 );

    /* errors */
    assert(( error( "fn main() { print 1 }" ) == std::pair< int, std::string >{ 1, "syntax error" } ));
    assert(( error( "fn main() {\n print x;\n}" ) == std::pair< int, std::string >{ 2, "undeclared variable x" } ));
    assert(( error( "fn main() { var x = x; }" ) == std::pair< int, std::string >{ 1, "undeclared variable x" } ));
    assert(( error( "fn main() { var a = 1;\n var a = 2; }" ) == std::pair< int, std::string >{ 2, "redeclared variable a" } ));
    assert(( error( "fn main() { g(); }" ) == std::pair< int, std::string >{ 1, "unknown function g" } ));
    assert(( error( "fn main() { }\nfn main() { }" ) == std::pair< int, std::string >{ 2, "duplicate function main" } ));
    assert(( error( "fn main() { f(1, 2); } fn f(x) { }" ) == std::pair< int, std::string >{ 1, "wrong number of arguments" } ));
    assert(( error( "fn main() { break; }" ) == std::pair< int, std::string >{ 1, "break outside of a loop" } ));
    assert(( error( "fn f() { }" ) == std::pair< int, std::string >{ 0, "no main" } ));
    assert(( error( "fn main() { print 1 < 2 < 3; }" ) == std::pair< int, std::string >{ 1, "syntax error" } ));
    assert(( error( "fn main() { try { } catch (e) { e = e + 1; } print e; }" ) ==
             std::pair< int, std::string >{ 1, "undeclared variable e" } ));

    return 0;
}
