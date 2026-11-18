#include <algorithm>
#include <cassert>
#include <cstdint>
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

/* In ‹d1_vm›, programs were encoded as bytes: each instruction is its
 * opcode (the value of ‹op› as a byte), followed – for the opcodes
 * with an operand (see ‹has_arg›) – by the operand as 8 bytes, least
 * significant first. Write a «disassembler»: a function ‹listing›
 * which turns such bytes into lines of text ‹OFFSET: INSTRUCTION›,
 * where ‹OFFSET› is the position of the opcode byte (in decimal) and
 * ‹INSTRUCTION› is written by ‹show›. Return ‹std::nullopt› if the bytes
 * contain an unknown opcode or end in the middle of an instruction. */

// std::optional< std::vector< std::string > > listing( const std::vector< std::uint8_t > &bytes );

std::optional< std::vector< std::string > > listing( const std::vector< std::uint8_t > &bytes )
{
    std::vector< std::string > out;

    for ( std::size_t pos = 0; pos < bytes.size(); )
    {
        std::size_t start = pos;
        if ( bytes[ pos ] > std::uint8_t( op::halt ) ) return std::nullopt;
        instr i{ op( bytes[ pos++ ] ) };

        if ( has_arg( i.o ) )
        {
            if ( pos + 8 > bytes.size() ) return std::nullopt;
            std::uint64_t v = 0;
            for ( int k = 0; k < 8; ++ k )
                v |= std::uint64_t( bytes[ pos++ ] ) << ( 8 * k );
            i.arg = long( v );
        }

        out.push_back( std::to_string( start ) + ": " + show( i ) );
    }

    return out;
}

int main()
{
    using v = std::vector< std::string >;
    using b = std::vector< std::uint8_t >;

    assert(( listing( {} ) == v{} ));
    assert(( listing( b{ 3, 19 } ) == v{ "0: add", "1: print" } ));
    assert(( listing( b{ 0, 5, 0, 0, 0, 0, 0, 0, 0, 12 } ) == v{ "0: push 5", "9: neg" } ));
    assert(( listing( b{ 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } ) == v{ "0: push -1" } ));
    assert(( listing( b{ 15, 17, 0, 1, 0, 0, 0, 0, 0, 0, 20 } ) == v{ "0: pop", "1: jump 256", "10: halt" } ));

    assert( !listing( b{ 21 } ) );
    assert( !listing( b{ 0, 1, 2 } ) );

    return 0;
}
