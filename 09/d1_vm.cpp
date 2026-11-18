#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/* This demo presents the virtual machine which most of the exercises
 * in this chapter use. Read the code of ‹execute› carefully: it is the
 * heart of every bytecode interpreter – a loop which fetches an
 * instruction, dispatches on its opcode with a ‹switch›, and performs
 * it on the «operand stack» and the «local slots». ‹pc› (the «program
 * counter») is the index of the next instruction; jumps simply assign
 * to it. Notice that nothing in the loop knows anything about the
 * source language: names, scopes, expressions and statements are all
 * gone. */

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

/* A hand-written program: the factorial of 10, using slot 0 for ‹n›
 * and slot 1 for the accumulator. The equivalent ‹tiny› code would be
 *
 *     var n = 10; var acc = 1;
 *     while ( 0 < n ) { acc = acc * n; n = n - 1; }
 *     print acc;
 *
 * Notice how the loop becomes a conditional jump «forward» (out of the
 * loop, when the condition is false) and an unconditional jump
 * «backward» (to re-test the condition). */

program factorial()
{
    return
    {
        { op::push, 10 }, { op::store, 0 },     /* 0, 1:  n = 10 */
        { op::push, 1 },  { op::store, 1 },     /* 2, 3:  acc = 1 */
        { op::push, 0 },  { op::load, 0 },      /* 4, 5:  0 < n ? */
        { op::lt },       { op::jz, 17 },       /* 6, 7:  if not, leave the loop */
        { op::load, 1 },  { op::load, 0 },      /* 8, 9 */
        { op::mul },      { op::store, 1 },     /* 10, 11: acc = acc * n */
        { op::load, 0 },  { op::push, 1 },      /* 12, 13 */
        { op::sub },      { op::store, 0 },     /* 14, 15: n = n - 1 */
        { op::jump, 4 },                        /* 16:     back to the test */
        { op::load, 1 },  { op::print },        /* 17, 18: print acc */
    };
}

/* ‘Bytecode’: the program as a sequence of bytes. Each instruction is
 * one opcode byte, followed (for the opcodes with an operand) by the
 * operand as 8 bytes, least significant first. Jump targets are still
 * instruction indices here; a real VM would use byte offsets, so that
 * it can execute the bytes directly, without decoding them first. */

std::vector< std::uint8_t > encode( const program &p )
{
    std::vector< std::uint8_t > out;
    for ( const auto &i : p )
    {
        out.push_back( std::uint8_t( i.o ) );
        if ( has_arg( i.o ) )
            for ( int k = 0; k < 8; ++ k )
                out.push_back( std::uint8_t( std::uint64_t( i.arg ) >> ( 8 * k ) ) );
    }
    return out;
}

std::optional< program > decode( const std::vector< std::uint8_t > &bytes )
{
    program p;
    for ( std::size_t pos = 0; pos < bytes.size(); )
    {
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
        p.push_back( i );
    }
    return p;
}

int main() /* demo */
{
    program p = factorial();
    stats st;
    assert(( execute( p, 2, &st ) == std::vector< long >{ 3628800 } ));
    assert( st.max_stack == 2 );
    assert( st.steps > 100 );

    /* errors are detected, not undefined behaviour */
    assert( !execute( { { op::add } } ) );                          /* underflow */
    assert( !execute( { { op::load, 5 } }, 2 ) );                   /* bad slot */
    assert( !execute( { { op::push, 1 }, { op::push, 0 }, { op::div } } ) );
    assert( !execute( { { op::jump, 0 } } ) );                      /* out of fuel */
    assert(( execute( { { op::push, 7 }, { op::print }, { op::halt }, { op::push, 1 }, { op::print } } ) ==
             std::vector< long >{ 7 } ));

    auto bytes = encode( p );
    assert( bytes.size() == 19 + 15 * 8 );  /* 19 opcodes, 15 of them with an operand */
    assert( decode( bytes ) == p );
    assert( show( p[ 7 ] ) == "jz 17" && show( p[ 6 ] ) == "lt" );

    return 0;
}
