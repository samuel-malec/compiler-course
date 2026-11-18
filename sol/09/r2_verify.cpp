#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <set>

/* The stack machine of this chapter (see ‹d1_vm› for a detailed
 * description). A program is a vector of instructions; each has an
 * opcode and an integer operand, which only some opcodes use:
 *
 *  • ‹push n› – push the constant ‹n›,
 *  • ‹load i› / ‹store i› – push the value of local slot ‹i› / pop a
 *    value into slot ‹i› (slots start as 0),
 *  • ‹add›, ‹sub›, ‹mul›, ‹div›, ‹mod›, ‹lt›, ‹le›, ‹eq›, ‹ne› – pop ‹b›,
 *    pop ‹a›, push ‹a op b› (comparisons give 1 or 0),
 *  • ‹neg›, ‹not› – replace the top ‹a› by ‹-a›, or by 1 if ‹a = 0› and
 *    0 otherwise,
 *  • ‹dup›, ‹pop›, ‹swap› – duplicate / drop / exchange the top values,
 *  • ‹jump n› – continue at instruction ‹n› (an index into the program),
 *  • ‹jz n› – pop a value; if it is zero, continue at instruction ‹n›,
 *  • ‹print› – pop a value and append it to the output,
 *  • ‹halt› – stop (so does running past the last instruction).
 *
 * ‹execute› runs a program and returns the output, or ‹std::nullopt› if
 * the program does something wrong: a stack underflow, a slot outside
 * of ‹0 … slots - 1›, a jump outside of the program, division by zero,
 * or more than ‹fuel› executed instructions. If ‹st› is given, it is
 * filled with statistics about the run. ‹show› writes an instruction
 * as text (e.g. ‹push 3›, ‹add›). */

enum class op
{
    push, load, store, add, sub, mul, div, mod, lt, le, eq, ne,
    neg, not_, dup, pop, swap, jump, jz, print, halt
};

struct instr
{
    op o;
    long arg = 0;
    bool operator==( const instr & ) const = default;
};

using program = std::vector< instr >;

struct stats
{
    std::size_t max_stack = 0;   /* the largest stack depth reached */
    long steps = 0;              /* the number of executed instructions */
};

const char *op_names[] = { "push", "load", "store", "add", "sub", "mul", "div", "mod",
                           "lt", "le", "eq", "ne", "neg", "not", "dup", "pop", "swap",
                           "jump", "jz", "print", "halt" };

bool has_arg( op o ) { return o == op::push || o == op::load || o == op::store || o == op::jump || o == op::jz; }

std::string show( const instr &i )
{
    std::string s = op_names[ int( i.o ) ];
    return has_arg( i.o ) ? s + " " + std::to_string( i.arg ) : s;
}

std::optional< std::vector< long > > execute( const program &p, std::size_t slots = 16,
                                              stats *st = nullptr, long fuel = 1000000 )
{
    std::vector< long > stack, locals( slots, 0 ), out;
    stats dummy;
    stats &s = st ? *st : dummy;
    s = {};

    auto pop = [ & ]( long &v ) { if ( stack.empty() ) return false; v = stack.back(); stack.pop_back(); return true; };

    for ( std::size_t pc = 0; pc < p.size(); )
    {
        if ( ++ s.steps > fuel ) return std::nullopt;
        const instr &i = p[ pc++ ];
        long a, b;

        switch ( i.o )
        {
            case op::push: stack.push_back( i.arg ); break;
            case op::load:
                if ( i.arg < 0 || std::size_t( i.arg ) >= slots ) return std::nullopt;
                stack.push_back( locals[ i.arg ] ); break;
            case op::store:
                if ( i.arg < 0 || std::size_t( i.arg ) >= slots || !pop( a ) ) return std::nullopt;
                locals[ i.arg ] = a; break;
            case op::neg: case op::not_:
                if ( !pop( a ) ) return std::nullopt;
                stack.push_back( i.o == op::neg ? -a : a == 0 ); break;
            case op::dup:
                if ( !pop( a ) ) return std::nullopt;
                stack.push_back( a ), stack.push_back( a ); break;
            case op::pop:
                if ( !pop( a ) ) return std::nullopt;
                break;
            case op::swap:
                if ( !pop( b ) || !pop( a ) ) return std::nullopt;
                stack.push_back( b ), stack.push_back( a ); break;
            case op::jump:
                if ( i.arg < 0 || std::size_t( i.arg ) > p.size() ) return std::nullopt;
                pc = i.arg; break;
            case op::jz:
                if ( i.arg < 0 || std::size_t( i.arg ) > p.size() || !pop( a ) ) return std::nullopt;
                if ( a == 0 ) pc = i.arg;
                break;
            case op::print:
                if ( !pop( a ) ) return std::nullopt;
                out.push_back( a ); break;
            case op::halt: return out;
            default:
                if ( !pop( b ) || !pop( a ) ) return std::nullopt;
                if ( ( i.o == op::div || i.o == op::mod ) && b == 0 ) return std::nullopt;
                switch ( i.o )
                {
                    case op::add: stack.push_back( a + b ); break;
                    case op::sub: stack.push_back( a - b ); break;
                    case op::mul: stack.push_back( a * b ); break;
                    case op::div: stack.push_back( a / b ); break;
                    case op::mod: stack.push_back( a % b ); break;
                    case op::lt:  stack.push_back( a < b ); break;
                    case op::le:  stack.push_back( a <= b ); break;
                    case op::eq:  stack.push_back( a == b ); break;
                    default:      stack.push_back( a != b ); break;
                }
        }

        s.max_stack = std::max( s.max_stack, stack.size() );
    }

    return out;
}

/* A VM which runs untrusted code (the JVM, WebAssembly engines) first
 * «verifies» it, so that the interpreter can skip run-time checks.
 * Write a verifier ‹verify( p, slots )› for our VM which accepts a
 * program only if, on «every» path from the start (considering both
 * outcomes of every ‹jz›, regardless of the actual values):
 *
 *  • the stack never underflows, and every instruction is always
 *    reached with the same stack depth (as in ‹p2_depth›),
 *  • every jump target is within ‹0 … p.size()›,
 *  • every ‹load› and ‹store› uses a slot within ‹0 … slots - 1›,
 *  • every ‹load i› is preceded by a ‹store i› – a slot is never read
 *    before it has been written («definite assignment», as in
 *    ‹05/p3_definite›, but on bytecode),
 *  • the stack is empty whenever the program stops (at ‹halt›, or by
 *    reaching the end, by falling off or jumping there).
 *
 * The analysis of definite assignment is a «data-flow analysis»: the
 * set of slots written on «all» paths to an instruction is the
 * «intersection» over its predecessors. Iterate until nothing changes
 * (the sets can only shrink, so this terminates). Unreachable
 * instructions are ignored. */

// bool verify( const program &p, std::size_t slots );

std::pair< int, int > effect( op o )
{
    switch ( o )
    {
        case op::push: case op::load: return { 0, 1 };
        case op::store: case op::pop: case op::print: case op::jz: return { 1, 0 };
        case op::neg: case op::not_: return { 1, 1 };
        case op::dup: return { 1, 2 };
        case op::swap: return { 2, 2 };
        case op::jump: case op::halt: return { 0, 0 };
        default: return { 2, 1 };
    }
}

bool verify( const program &p, std::size_t slots )
{
    std::size_t n = p.size();
    std::vector< long > depth( n + 1, -1 );
    std::vector< std::set< long > > written( n + 1 );   /* definitely written before */
    std::vector< bool > seen( n + 1, false );
    std::vector< std::size_t > work;

    /* flow ‹d› and ‹w› into ‹t›; false if inconsistent */
    auto flow = [ & ]( long t, long d, const std::set< long > &w ) -> bool
    {
        if ( t < 0 || std::size_t( t ) > n ) return false;
        if ( !seen[ t ] )
        {
            seen[ t ] = true, depth[ t ] = d, written[ t ] = w;
            work.push_back( t );
            return true;
        }
        if ( depth[ t ] != d ) return false;
        std::set< long > meet;
        for ( long s : written[ t ] ) if ( w.contains( s ) ) meet.insert( s );
        if ( meet.size() != written[ t ].size() )
        {
            written[ t ] = meet;
            work.push_back( t );
        }
        return true;
    };

    std::set< long > none;
    if ( !flow( 0, 0, none ) ) return false;

    while ( !work.empty() )
    {
        std::size_t pc = work.back();
        work.pop_back();
        long d = depth[ pc ];
        std::set< long > w = written[ pc ];

        if ( pc == n )
        {
            if ( d != 0 ) return false;
            continue;
        }

        const instr &i = p[ pc ];
        auto [ pops, pushes ] = effect( i.o );
        if ( d < pops ) return false;
        long after = d - pops + pushes;

        if ( i.o == op::load || i.o == op::store )
        {
            if ( i.arg < 0 || std::size_t( i.arg ) >= slots ) return false;
            if ( i.o == op::load && !w.contains( i.arg ) ) return false;
            if ( i.o == op::store ) w.insert( i.arg );
        }

        bool ok = true;
        if ( i.o == op::halt ) ok = after == 0;
        else if ( i.o == op::jump ) ok = flow( i.arg, after, w );
        else if ( i.o == op::jz ) ok = flow( i.arg, after, w ) && flow( pc + 1, after, w );
        else ok = flow( pc + 1, after, w );
        if ( !ok ) return false;
    }

    return true;
}

int main()
{
    using p = program;

    assert( verify( {}, 0 ) );
    assert( verify( p{ { op::push, 1 }, { op::print } }, 0 ) );
    assert( verify( p{ { op::push, 1 }, { op::store, 0 }, { op::load, 0 }, { op::print } }, 1 ) );

    p loop{ { op::push, 5 }, { op::store, 0 }, { op::load, 0 }, { op::jz, 9 },
            { op::load, 0 }, { op::push, 1 }, { op::sub }, { op::store, 0 }, { op::jump, 2 } };
    assert( verify( loop, 1 ) );

    /* slot 1 is only written on one of the branches */
    p branch{ { op::push, 1 }, { op::jz, 4 }, { op::push, 2 }, { op::store, 1 },
              { op::load, 1 }, { op::print } };
    assert( !verify( branch, 2 ) );
    branch[ 1 ].arg = 3;   /* now ‹store 1› is on both paths */
    branch[ 2 ] = { op::push, 3 };
    assert( !verify( branch, 2 ) );   /* … but the depths differ at 3 */
    p both{ { op::push, 1 }, { op::jz, 5 }, { op::push, 2 }, { op::store, 1 }, { op::jump, 7 },
            { op::push, 3 }, { op::store, 1 }, { op::load, 1 }, { op::print } };
    assert( verify( both, 2 ) );

    /* written in the loop body only: not definitely assigned after the loop */
    p after{ { op::push, 0 }, { op::jz, 4 }, { op::push, 1 }, { op::store, 0 }, { op::load, 0 }, { op::print } };
    assert( !verify( after, 1 ) );

    assert( !verify( p{ { op::load, 0 }, { op::print } }, 1 ) );
    assert( !verify( p{ { op::push, 1 }, { op::store, 3 } }, 2 ) );
    assert( !verify( p{ { op::push, 1 } }, 0 ) );                 /* non-empty stack at the end */
    assert( !verify( p{ { op::push, 1 }, { op::halt } }, 0 ) );
    assert( !verify( p{ { op::jump, 5 } }, 0 ) );
    assert( verify( p{ { op::jump, 2 }, { op::load, 7 }, { op::halt } }, 0 ) ); /* unreachable */

    return 0;
}
