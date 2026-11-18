#include <algorithm>
#include <cassert>
#include <climits>
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

/* The encoding of ‹d1_vm› spends 8 bytes on every operand, although
 * most operands are small. Real bytecode formats (WebAssembly, DWARF,
 * Android's DEX) use «LEB128», a variable-length encoding: the value is
 * split into groups of 7 bits, least significant first, and each group
 * is stored in one byte whose top bit says whether more bytes follow.
 *
 *  • «Unsigned» LEB128 encodes a non-negative value; it stops as soon
 *    as the remaining value is zero (so 0 is the single byte ‹0x00›,
 *    127 is ‹0x7f›, 128 is ‹0x80 0x01›).
 *  • «Signed» LEB128 encodes two's complement values; it stops when the
 *    remaining value (after an arithmetic shift) is 0 and the sign bit
 *    of the last group (bit 6) is clear, or the remaining value is -1
 *    and that bit is set (so -1 is ‹0x7f›, 63 is ‹0x3f›, 64 is
 *    ‹0xc0 0x00›, -65 is ‹0xbf 0x7f›).
 *
 * Write ‹encode› and ‹decode› for this format: each instruction is its
 * opcode byte, followed – for ‹push› – by the operand in signed
 * LEB128, and – for ‹load›, ‹store›, ‹jump› and ‹jz› – by the operand in
 * unsigned LEB128. ‹encode› fails (returns ‹std::nullopt›) if an
 * unsigned operand is negative. ‹decode› fails for an unknown opcode,
 * for bytes which end in the middle of an instruction, and for an
 * operand which does not fit into a ‹long› (including encodings longer
 * than 10 bytes). */

// std::optional< std::vector< std::uint8_t > > encode( const program &p );
// std::optional< program > decode( const std::vector< std::uint8_t > &bytes );

bool is_unsigned( op o ) { return o == op::load || o == op::store || o == op::jump || o == op::jz; }

void put_uleb( std::vector< std::uint8_t > &out, std::uint64_t v )
{
    do
    {
        std::uint8_t byte = v & 0x7f;
        v >>= 7;
        out.push_back( v ? byte | 0x80 : byte );
    } while ( v );
}

void put_sleb( std::vector< std::uint8_t > &out, long v )
{
    while ( true )
    {
        std::uint8_t byte = v & 0x7f;
        v >>= 7; /* arithmetic shift */
        bool done = ( v == 0 && !( byte & 0x40 ) ) || ( v == -1 && ( byte & 0x40 ) );
        out.push_back( done ? byte : byte | 0x80 );
        if ( done ) return;
    }
}

std::optional< std::vector< std::uint8_t > > encode( const program &p )
{
    std::vector< std::uint8_t > out;
    for ( const auto &i : p )
    {
        out.push_back( std::uint8_t( i.o ) );
        if ( i.o == op::push )
            put_sleb( out, i.arg );
        else if ( is_unsigned( i.o ) )
        {
            if ( i.arg < 0 ) return std::nullopt;
            put_uleb( out, i.arg );
        }
    }
    return out;
}

/* Read the 7-bit groups; returns the raw bits and the number of bits. */
std::optional< std::pair< std::uint64_t, int > > get_leb( const std::vector< std::uint8_t > &b, std::size_t &pos )
{
    std::uint64_t v = 0;
    int shift = 0;
    for ( int n = 0; ; ++ n )
    {
        if ( pos >= b.size() || n == 10 ) return std::nullopt;
        std::uint8_t byte = b[ pos++ ];
        if ( shift == 63 && ( byte & 0x7e ) && ( byte & 0x7f ) != 0x7f ) return std::nullopt;
        if ( shift < 64 ) v |= std::uint64_t( byte & 0x7f ) << shift;
        shift += 7;
        if ( !( byte & 0x80 ) ) return std::pair{ v, shift };
    }
}

std::optional< program > decode( const std::vector< std::uint8_t > &bytes )
{
    program p;
    for ( std::size_t pos = 0; pos < bytes.size(); )
    {
        if ( bytes[ pos ] > std::uint8_t( op::halt ) ) return std::nullopt;
        instr i{ op( bytes[ pos++ ] ) };

        if ( i.o == op::push )
        {
            auto r = get_leb( bytes, pos );
            if ( !r ) return std::nullopt;
            auto [ v, bits ] = *r;
            if ( bits < 64 && ( v >> ( bits - 1 ) ) & 1 )  /* sign-extend */
                v |= ~std::uint64_t( 0 ) << bits;
            i.arg = long( v );
        }
        else if ( is_unsigned( i.o ) )
        {
            auto r = get_leb( bytes, pos );
            if ( !r || r->first > std::uint64_t( LONG_MAX ) ) return std::nullopt;
            if ( r->second > 63 && ( bytes[ pos - 1 ] & 0x7f ) > 1 ) return std::nullopt;
            i.arg = long( r->first );
        }
        p.push_back( i );
    }
    return p;
}

int main()
{
    using b = std::vector< std::uint8_t >;
    auto enc = []( instr i ) { return *encode( { i } ); };

    assert(( enc( { op::add } ) == b{ 3 } ));
    assert(( enc( { op::push, 0 } ) == b{ 0, 0x00 } ));
    assert(( enc( { op::push, 63 } ) == b{ 0, 0x3f } ));
    assert(( enc( { op::push, 64 } ) == b{ 0, 0xc0, 0x00 } ));
    assert(( enc( { op::push, -1 } ) == b{ 0, 0x7f } ));
    assert(( enc( { op::push, -64 } ) == b{ 0, 0x40 } ));
    assert(( enc( { op::push, -65 } ) == b{ 0, 0xbf, 0x7f } ));
    assert(( enc( { op::push, 300 } ) == b{ 0, 0xac, 0x02 } ));
    assert(( enc( { op::load, 127 } ) == b{ 1, 0x7f } ));
    assert(( enc( { op::load, 128 } ) == b{ 1, 0x80, 0x01 } ));
    assert(( enc( { op::jump, 0 } ) == b{ 17, 0x00 } ));
    assert( !encode( { { op::store, -1 } } ) );

    program all{ { op::push, LONG_MAX }, { op::push, LONG_MIN }, { op::push, -123456789 },
                 { op::store, 5 }, { op::jz, 1000000 }, { op::load, LONG_MAX }, { op::print }, { op::halt } };
    auto bytes = encode( all );
    assert( bytes && decode( *bytes ) == all );
    assert( encode( { { op::push, LONG_MAX } } )->size() == 11 );

    assert( !decode( b{ 21 } ) );
    assert( !decode( b{ 0 } ) );
    assert( !decode( b{ 0, 0x80 } ) );
    assert( !decode( b{ 1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01 } ) ); /* 2⁶⁴ - 1 */
    assert( !decode( b{ 0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00 } ) );
    assert(( decode( b{ 3, 4, 19 } ) == program{ { op::add }, { op::sub }, { op::print } } ));

    return 0;
}
