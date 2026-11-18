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
#include <set>

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

/* ── Lexer ── */

struct token
{
    enum kind_t { ident, number, keyword, punct, end } kind;
    std::string text;
    int line;
};

std::vector< token > lex( std::string_view s )
{
    static const std::set< std::string > keywords =
        { "fn", "var", "if", "else", "while", "return", "break", "continue", "print", "throw", "try", "catch" };
    std::vector< token > out;
    int line = 1;
    std::size_t i = 0;
    auto alpha = []( char c ) { return std::isalpha( static_cast< unsigned char >( c ) ) || c == '_'; };
    auto digit = []( char c ) { return std::isdigit( static_cast< unsigned char >( c ) ); };

    while ( i < s.size() )
    {
        char c = s[ i ];
        if ( c == '\n' ) { ++ line, ++ i; continue; }
        if ( c == ' ' || c == '\t' || c == '\r' ) { ++ i; continue; }
        if ( s.substr( i, 2 ) == "//" )
        {
            while ( i < s.size() && s[ i ] != '\n' ) ++ i;
            continue;
        }
        std::size_t start = i;
        if ( alpha( c ) )
        {
            while ( i < s.size() && ( alpha( s[ i ] ) || digit( s[ i ] ) ) ) ++ i;
            std::string w( s.substr( start, i - start ) );
            out.push_back( { keywords.contains( w ) ? token::keyword : token::ident, w, line } );
            continue;
        }
        if ( digit( c ) )
        {
            while ( i < s.size() && digit( s[ i ] ) ) ++ i;
            out.push_back( { token::number, std::string( s.substr( start, i - start ) ), line } );
            continue;
        }
        static const std::vector< std::string > two = { "&&", "||", "==", "!=", "<=", ">=" };
        if ( std::find( two.begin(), two.end(), s.substr( i, 2 ) ) != two.end() )
        {
            out.push_back( { token::punct, std::string( s.substr( i, 2 ) ), line } );
            i += 2;
            continue;
        }
        if ( std::string_view( "+-*/%<>!=(){},;" ).find( c ) == std::string_view::npos )
            throw compile_error{ line, "syntax error" };
        out.push_back( { token::punct, std::string( 1, c ), line } );
        ++ i;
    }
    out.push_back( { token::end, "", out.empty() ? line : out.back().line } );
    return out;
}

/* ── Parser and code generator (one pass, with backpatching) ── */

class compiler
{
    std::vector< token > toks;
    std::size_t pos = 0;

    struct signature { int index, params; };
    std::map< std::string, signature > funcs;
    module mod;
    std::set< std::string > defined;

    /* the function being compiled */
    function *fn = nullptr;
    std::vector< std::map< std::string, int > > scopes;
    struct loop_info { std::size_t top; std::vector< std::size_t > breaks; };
    std::vector< loop_info > loops;
    int line = 0;   /* the line of the current statement */

    [[noreturn]] void fail( int l, std::string msg ) { throw compile_error{ l, std::move( msg ) }; }
    [[noreturn]] void syntax() { fail( peek().line, "syntax error" ); }

    const token &peek( int k = 0 ) const { return toks[ std::min( pos + k, toks.size() - 1 ) ]; }
    bool is( std::string_view t, int k = 0 ) const
    {
        const token &x = peek( k );
        return ( x.kind == token::punct || x.kind == token::keyword ) && x.text == t;
    }
    bool accept( std::string_view t ) { if ( is( t ) ) { ++ pos; return true; } return false; }
    void expect( std::string_view t ) { if ( !accept( t ) ) syntax(); }
    const token &expect_ident()
    {
        if ( peek().kind != token::ident ) syntax();
        return toks[ pos++ ];
    }

    std::size_t emit( op o, long arg = 0, long arg2 = 0 )
    {
        fn->code.push_back( { o, arg, arg2 } );
        fn->lines.push_back( line );
        return fn->code.size() - 1;
    }
    std::size_t here() const { return fn->code.size(); }
    void patch( std::size_t at, std::size_t target ) { fn->code[ at ].arg = target; }

    int declare( const token &name )
    {
        if ( scopes.back().contains( name.text ) ) fail( name.line, "redeclared variable " + name.text );
        int slot = fn->slots++;
        scopes.back()[ name.text ] = slot;
        return slot;
    }
    int lookup( const token &name )
    {
        for ( auto it = scopes.rbegin(); it != scopes.rend(); ++ it )
            if ( auto v = it->find( name.text ); v != it->end() ) return v->second;
        fail( name.line, "undeclared variable " + name.text );
    }

    /* expressions */

    void call( const token &name, bool tail )
    {
        auto f = funcs.find( name.text );
        if ( f == funcs.end() ) fail( name.line, "unknown function " + name.text );
        expect( "(" );
        int n = 0;
        if ( !is( ")" ) )
            do { expr(); ++ n; } while ( accept( "," ) );
        expect( ")" );
        if ( n != f->second.params ) fail( name.line, "wrong number of arguments" );
        emit( tail ? op::tailcall : op::call, f->second.index );
    }

    void primary()
    {
        const token &t = peek();
        if ( t.kind == token::number )
        {
            ++ pos;
            emit( op::push, std::stol( t.text ) );
        }
        else if ( t.kind == token::ident )
        {
            ++ pos;
            if ( is( "(" ) ) call( t, false );
            else emit( op::load, lookup( t ) );
        }
        else if ( accept( "(" ) )
        {
            expr();
            expect( ")" );
        }
        else syntax();
    }

    void unary()
    {
        if ( accept( "-" ) ) { unary(); emit( op::neg ); }
        else if ( accept( "!" ) ) { unary(); emit( op::not_ ); }
        else primary();
    }

    template< typename F >
    void left_assoc( F operand, const std::map< std::string, op > &ops )
    {
        operand();
        while ( peek().kind == token::punct && ops.contains( peek().text ) )
        {
            op o = ops.at( toks[ pos++ ].text );
            operand();
            emit( o );
        }
    }

    void term() { left_assoc( [ & ] { unary(); }, { { "*", op::mul }, { "/", op::div }, { "%", op::mod } } ); }
    void sum()  { left_assoc( [ & ] { term(); }, { { "+", op::add }, { "-", op::sub } } ); }

    void cmp()
    {
        sum();
        for ( auto [ t, o, flip ] : { std::tuple{ "<", op::lt, false }, { "<=", op::le, false },
                                      { ">", op::lt, true }, { ">=", op::le, true } } )
            if ( accept( t ) )
            {
                sum();
                if ( flip ) emit( op::swap );
                emit( o );
                return;
            }
    }

    void eq()
    {
        cmp();
        if ( accept( "==" ) ) { cmp(); emit( op::eq ); }
        else if ( accept( "!=" ) ) { cmp(); emit( op::ne ); }
    }

    /* ‹a && b› → a; jz F; b; not; not; jump E; F: push 0; E: */
    void logical( bool is_and )
    {
        auto operand = [ & ] { if ( is_and ) eq(); else logical( true ); };
        operand();
        std::vector< std::size_t > shortcut;
        bool any = false;
        while ( accept( is_and ? "&&" : "||" ) )
        {
            any = true;
            if ( is_and ) shortcut.push_back( emit( op::jz ) );
            else { emit( op::not_ ); shortcut.push_back( emit( op::jz ) ); }
            operand();
        }
        if ( !any ) return;
        emit( op::not_ ), emit( op::not_ );
        std::size_t done = emit( op::jump );
        for ( auto j : shortcut ) patch( j, here() );
        emit( op::push, is_and ? 0 : 1 );
        patch( done, here() );
    }

    void expr() { logical( false ); }

    /* statements */

    void block( bool new_scope = true )
    {
        expect( "{" );
        if ( new_scope ) scopes.emplace_back();
        while ( !is( "}" ) )
        {
            if ( peek().kind == token::end ) syntax();
            stmt();
        }
        ++ pos;
        if ( new_scope ) scopes.pop_back();
    }

    void if_stmt( int l )
    {
        expect( "(" );
        expr();
        expect( ")" );
        std::size_t jz = emit( op::jz );
        block();
        if ( accept( "else" ) )
        {
            line = l;
            std::size_t jump = emit( op::jump );
            patch( jz, here() );
            if ( accept( "if" ) ) { line = toks[ pos - 1 ].line; if_stmt( line ); }
            else block();
            patch( jump, here() );
        }
        else patch( jz, here() );
    }

    void stmt()
    {
        line = peek().line;
        const token &t = peek();

        if ( accept( "var" ) )
        {
            const token &name = expect_ident();
            expect( "=" );
            expr();
            expect( ";" );
            emit( op::store, declare( name ) );   /* visible only after the initializer */
        }
        else if ( t.kind == token::ident && is( "=", 1 ) )
        {
            pos += 2;
            expr();
            expect( ";" );
            emit( op::store, lookup( t ) );
        }
        else if ( accept( "if" ) ) if_stmt( t.line );
        else if ( accept( "while" ) )
        {
            std::size_t top = here();
            expect( "(" );
            expr();
            expect( ")" );
            std::size_t jz = emit( op::jz );
            loops.push_back( { top, {} } );
            block();
            line = t.line;   /* the jump belongs to the ‹while› */
            emit( op::jump, top );
            patch( jz, here() );
            for ( auto b : loops.back().breaks ) patch( b, here() );
            loops.pop_back();
        }
        else if ( accept( "return" ) )
        {
            if ( peek().kind == token::ident && is( "(", 1 ) && tail_call_follows() )
            {
                const token &name = toks[ pos++ ];
                call( name, true );
            }
            else
            {
                expr();
                emit( op::ret );
            }
            expect( ";" );
        }
        else if ( accept( "break" ) || accept( "continue" ) )
        {
            bool brk = toks[ pos - 1 ].text == "break";
            if ( loops.empty() ) fail( t.line, brk ? "break outside of a loop" : "continue outside of a loop" );
            expect( ";" );
            if ( brk ) loops.back().breaks.push_back( emit( op::jump ) );
            else emit( op::jump, loops.back().top );
        }
        else if ( accept( "print" ) ) { expr(); expect( ";" ); emit( op::print ); }
        else if ( accept( "throw" ) ) { expr(); expect( ";" ); emit( op::throw_ ); }
        else if ( accept( "try" ) )
        {
            std::size_t from = here();
            block();
            std::size_t to = here();
            line = t.line;
            std::size_t skip = emit( op::jump );
            fn->handlers.push_back( { from, to, here() } );   /* inner ‹try›s were added before */
            expect( "catch" );
            expect( "(" );
            const token &name = expect_ident();
            expect( ")" );
            line = t.line;
            scopes.emplace_back();
            emit( op::store, declare( name ) );
            block( false );
            scopes.pop_back();
            patch( skip, here() );
        }
        else if ( is( "{" ) ) block();
        else
        {
            expr();
            expect( ";" );
            emit( op::pop );
        }
    }

    /* is ‹return f( … )› followed directly by ‹;›? (then it is a tail call) */
    bool tail_call_follows() const
    {
        int depth = 0;
        for ( std::size_t k = pos + 1; k < toks.size(); ++ k )
        {
            if ( toks[ k ].kind != token::punct ) continue;
            if ( toks[ k ].text == "(" ) ++ depth;
            else if ( toks[ k ].text == ")" && -- depth == 0 )
                return k + 1 < toks.size() && toks[ k + 1 ].kind == token::punct && toks[ k + 1 ].text == ";";
        }
        return false;
    }

    void function_def()
    {
        expect( "fn" );
        const token &name = expect_ident();
        if ( defined.contains( name.text ) ) fail( name.line, "duplicate function " + name.text );
        defined.insert( name.text );
        fn = &mod[ funcs.at( name.text ).index ];

        scopes = { {} };
        loops.clear();
        expect( "(" );
        if ( !is( ")" ) )
            do declare( expect_ident() ); while ( accept( "," ) );
        expect( ")" );
        if ( name.text == "main" && fn->params != 0 ) fail( name.line, "wrong number of arguments" );

        block( false );
        line = toks[ pos - 1 ].line;
        emit( op::push, 0 );
        emit( op::ret );
    }

public:
    explicit compiler( std::string_view src ) : toks( lex( src ) ) {}

    module compile()
    {
        /* pre-scan: the signatures of all functions; ‹main› first */
        std::vector< std::pair< std::string, int > > sigs;
        int depth = 0;
        for ( std::size_t k = 0; k < toks.size(); ++ k )
        {
            const token &t = toks[ k ];
            if ( t.kind == token::punct && t.text == "{" ) ++ depth;
            if ( t.kind == token::punct && t.text == "}" ) -- depth;
            if ( depth != 0 || t.kind != token::keyword || t.text != "fn" ) continue;
            if ( k + 2 >= toks.size() || toks[ k + 1 ].kind != token::ident || toks[ k + 2 ].text != "(" ) continue;
            int n = 0;
            for ( std::size_t j = k + 3; j < toks.size() && toks[ j ].text != ")"; ++ j )
                if ( toks[ j ].kind == token::ident ) ++ n;
            bool seen = false;
            for ( auto &s : sigs ) seen |= s.first == toks[ k + 1 ].text;
            if ( !seen ) sigs.emplace_back( toks[ k + 1 ].text, n );
        }
        std::stable_partition( sigs.begin(), sigs.end(), []( auto &s ) { return s.first == "main"; } );
        bool has_main = !sigs.empty() && sigs[ 0 ].first == "main";
        if ( !has_main ) mod.push_back( { "main" } );   /* placeholder; reported at the end */
        for ( auto &[ name, n ] : sigs )
        {
            funcs[ name ] = { int( mod.size() ), n };
            mod.push_back( { name, n, 0 } );   /* ‹declare› counts the slots */
        }

        while ( peek().kind != token::end ) function_def();
        if ( !has_main ) fail( 0, "no main" );
        return mod;
    }
};

module compile( std::string_view source )
{
    return compiler( source ).compile();
}

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
