#include <algorithm>
#include <cassert>
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

/* Writing jump targets as instruction indices by hand is error-prone
 * (look at ‹d1_vm›). An «assembler» lets the programmer write symbolic
 * «labels» instead. Write a function ‹assemble› which translates text
 * like this:
 *
 *         push 10
 *         store 0          ; n = 10
 *     loop:
 *         load 0
 *         jz done
 *         load 0
 *         print
 *         load 0
 *         push 1
 *         sub
 *         store 0
 *         jump loop
 *     done:
 *
 * into a VM program. Each line contains (after removing a comment,
 * which starts with ‹;›, and surrounding whitespace) either nothing, a
 * label definition ‹NAME:›, or an instruction: an opcode name (as
 * written by ‹show›) and, for the opcodes with an operand, the operand.
 * The operand of ‹push›, ‹load› and ‹store› is an integer; that of
 * ‹jump› and ‹jz› is a label name (letters, digits and ‹_›, starting with
 * a letter or ‹_›). A label refers to the index of the next instruction
 * after it (several labels may refer to the same instruction, and a
 * label at the end refers to the end of the program). Labels may be used
 * before they are defined, so you need two passes (or backpatching).
 *
 * Return ‹std::nullopt› for an unknown opcode, a missing or superfluous
 * operand, a malformed integer, a label defined twice, or a jump to an
 * undefined label. */

// std::optional< program > assemble( std::string_view text );

int main()
{
    auto prog = assemble( "        push 10\n"
                          "        store 0          ; n = 10\n"
                          "    loop:\n"
                          "        load 0\n"
                          "        jz done\n"
                          "        load 0\n"
                          "        print\n"
                          "        load 0\n"
                          "        push 1\n"
                          "        sub\n"
                          "        store 0\n"
                          "        jump loop\n"
                          "    done:\n" );
    assert( prog );
    assert( prog->size() == 11 );
    assert(( ( *prog )[ 3 ] == instr{ op::jz, 11 } ));
    assert(( ( *prog )[ 10 ] == instr{ op::jump, 2 } ));
    assert(( execute( *prog )->size() == 10 ));

    assert(( assemble( "a: b:\n" ) == std::nullopt ));
    assert(( assemble( "a:\nb:\njump a\njump b\n" ) == program{ { op::jump, 0 }, { op::jump, 0 } } ));
    assert(( assemble( "push -5 ; negative\n  neg  \nprint" ) ==
             program{ { op::push, -5 }, { op::neg }, { op::print } } ));
    assert(( assemble( "" ) == program{} ));

    assert( !assemble( "frob" ) );
    assert( !assemble( "push" ) );
    assert( !assemble( "add 3" ) );
    assert( !assemble( "push x" ) );
    assert( !assemble( "push 1x" ) );
    assert( !assemble( "x:\nx:" ) );
    assert( !assemble( "jump nowhere" ) );
    assert( !assemble( "jz 3" ) );

    return 0;
}
