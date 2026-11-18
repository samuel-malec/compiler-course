#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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

/* A VM which knows the maximal stack depth of a program in advance can
 * allocate the stack once, and skip the overflow checks (the JVM
 * requires each method to declare its ‹max_stack›). Write a function
 * ‹max_depth› which computes it «statically», without running the
 * program: follow all possible paths through the code, tracking the
 * depth of the stack «before» each instruction. ‹jz› has two
 * successors (the next instruction and the target), ‹jump› only the
 * target, ‹halt› none; a jump to the very end of the program (index
 * ‹p.size()›) means stopping. The depth before the first instruction is
 * 0.
 *
 * Return ‹std::nullopt› if the program is not well-behaved: some path
 * reaches an instruction which would underflow the stack, a jump
 * target is outside of ‹0 … p.size()›, or two paths reach the same
 * instruction with different stack depths. Instructions which no
 * path reaches are ignored. Each instruction should be processed only
 * a bounded number of times (once is enough – why?). */

// std::optional< std::size_t > max_depth( const program &p );

int main()
{
    using p = program;

    assert( max_depth( {} ) == 0 );
    assert( max_depth( p{ { op::push, 1 }, { op::push, 2 }, { op::add }, { op::print } } ) == 2 );
    assert( max_depth( p{ { op::push, 1 }, { op::dup }, { op::dup }, { op::add }, { op::add }, { op::print } } ) == 3 );

    /* if-expression: both branches push one value */
    p cond{ { op::push, 1 }, { op::jz, 4 }, { op::push, 10 }, { op::jump, 5 }, { op::push, 20 }, { op::print } };
    assert( max_depth( cond ) == 1 );

    /* a loop whose body keeps the depth */
    p loop{ { op::push, 5 }, { op::store, 0 }, { op::load, 0 }, { op::jz, 8 },
            { op::load, 0 }, { op::push, 1 }, { op::sub }, { op::store, 0 }, { op::jump, 2 } };
    loop[ 3 ].arg = 9;
    assert( max_depth( loop ) == 2 );

    /* unreachable code is ignored */
    assert( max_depth( p{ { op::jump, 2 }, { op::add }, { op::push, 1 }, { op::pop } } ) == 1 );
    assert( max_depth( p{ { op::halt }, { op::add } } ) == 0 );

    assert( !max_depth( p{ { op::add } } ) );
    assert( !max_depth( p{ { op::push, 1 }, { op::jz, 7 } } ) );
    /* a loop which pushes a value in every iteration */
    assert( !max_depth( p{ { op::push, 1 }, { op::jump, 0 } } ) );
    /* branches with different depths meet */
    assert( !max_depth( p{ { op::push, 1 }, { op::jz, 3 }, { op::push, 2 }, { op::print } } ) );

    return 0;
}
