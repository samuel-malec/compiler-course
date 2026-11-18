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

/* A «peephole optimizer» looks at short windows of consecutive
 * instructions and replaces them with something cheaper. Write a
 * function ‹peephole› which applies these rules (‹a›, ‹b›, ‹c› are
 * constants, ‹i› a slot, ‹BIN› any of ‹add sub mul div mod lt le eq ne›):
 *
 *  1. ‹push a, push b, BIN› → ‹push (a BIN b)› – except for ‹div› and ‹mod›
 *     with ‹b = 0›, which must stay (to fail at run time),
 *  2. ‹push a, neg› → ‹push -a›; ‹push a, not› → ‹push (a = 0 ? 1 : 0)›,
 *  3. ‹push 0, add›, ‹push 0, sub›, ‹push 1, mul›, ‹push 1, div› → nothing,
 *  4. ‹push a, pop›, ‹load i, pop›, ‹dup, pop›, ‹swap, swap› → nothing,
 *  5. ‹store i, load i› → ‹dup, store i›,
 *  6. ‹push c, jz n› → ‹jump n› if ‹c = 0›, nothing otherwise,
 *  7. ‹jump n› where ‹n› is the index of the next instruction → nothing.
 *
 * A window may only be rewritten if none of its instructions «except
 * the first» is the target of a jump (otherwise, a path which jumps
 * into the middle of the window would see a different program). When
 * instructions are removed, all jump targets must be adjusted; a jump
 * to a removed instruction goes to the first instruction after it
 * which is kept (or to the end of the program).
 *
 * To make the result deterministic: repeatedly find the «first»
 * position (the smallest index) at which some rule applies, apply the
 * first applicable rule (in the order above) there, and start again,
 * until no rule applies anywhere. */

// program peephole( const program &p );

bool is_bin( op o )
{
    return o == op::add || o == op::sub || o == op::mul || o == op::div || o == op::mod ||
           o == op::lt || o == op::le || o == op::eq || o == op::ne;
}

long fold( op o, long a, long b )
{
    switch ( o )
    {
        case op::add: return a + b;
        case op::sub: return a - b;
        case op::mul: return a * b;
        case op::div: return a / b;
        case op::mod: return a % b;
        case op::lt: return a < b;
        case op::le: return a <= b;
        case op::eq: return a == b;
        default: return a != b;
    }
}

/* Try the rules at position ‹i›; on success, return the length of the
 * window and its replacement. */
std::optional< std::pair< std::size_t, program > > rewrite( const program &p, std::size_t i,
                                                            const std::vector< bool > &target )
{
    auto avail = [ & ]( std::size_t n )  /* a window of ‹n› instructions is allowed */
    {
        if ( i + n > p.size() ) return false;
        for ( std::size_t k = i + 1; k < i + n; ++ k )
            if ( target[ k ] ) return false;
        return true;
    };
    auto is = [ & ]( std::size_t k, op o ) { return p[ i + k ].o == o; };

    if ( avail( 3 ) && is( 0, op::push ) && is( 1, op::push ) && is_bin( p[ i + 2 ].o ) &&
         !( ( is( 2, op::div ) || is( 2, op::mod ) ) && p[ i + 1 ].arg == 0 ) )
        return std::pair{ 3, program{ { op::push, fold( p[ i + 2 ].o, p[ i ].arg, p[ i + 1 ].arg ) } } };

    if ( !avail( 2 ) )
        return std::nullopt;

    const instr &a = p[ i ], &b = p[ i + 1 ];

    if ( a.o == op::push && b.o == op::neg ) return std::pair{ 2, program{ { op::push, -a.arg } } };
    if ( a.o == op::push && b.o == op::not_ ) return std::pair{ 2, program{ { op::push, a.arg == 0 } } };
    if ( a.o == op::push && ( ( a.arg == 0 && ( b.o == op::add || b.o == op::sub ) ) ||
                              ( a.arg == 1 && ( b.o == op::mul || b.o == op::div ) ) ) )
        return std::pair{ 2, program{} };
    if ( b.o == op::pop && ( a.o == op::push || a.o == op::load || a.o == op::dup ) ) return std::pair{ 2, program{} };
    if ( a.o == op::swap && b.o == op::swap ) return std::pair{ 2, program{} };
    if ( a.o == op::store && b.o == op::load && a.arg == b.arg )
        return std::pair{ 2, program{ { op::dup }, a } };
    if ( a.o == op::push && b.o == op::jz )
        return std::pair{ 2, a.arg == 0 ? program{ { op::jump, b.arg } } : program{} };
    return std::nullopt;
}

program peephole( const program &input )
{
    program p = input;

    while ( true )
    {
        std::vector< bool > target( p.size() + 1, false );
        for ( const auto &i : p )
            if ( ( i.o == op::jump || i.o == op::jz ) && i.arg >= 0 && std::size_t( i.arg ) <= p.size() )
                target[ i.arg ] = true;

        std::optional< std::pair< std::size_t, program > > r;
        std::size_t at = 0;
        for ( ; at < p.size() && !r; ++ at )
        {
            if ( p[ at ].o == op::jump && p[ at ].arg == long( at + 1 ) )
                r = std::pair{ std::size_t( 1 ), program{} };
            else
                r = rewrite( p, at, target );
            if ( r ) break;
        }
        if ( !r ) return p;

        auto [ len, repl ] = *r;

        /* new index of each old instruction: those in the window map to
         * the start of the replacement; targets of removed instructions
         * move to the next kept one */
        std::vector< long > remap( p.size() + 1 );
        long delta = long( repl.size() ) - long( len );
        for ( std::size_t k = 0; k <= p.size(); ++ k )
            remap[ k ] = k <= at ? k : k < at + len ? at : k + delta;

        program out( p.begin(), p.begin() + at );
        out.insert( out.end(), repl.begin(), repl.end() );
        out.insert( out.end(), p.begin() + at + len, p.end() );
        for ( auto &i : out )
            if ( ( i.o == op::jump || i.o == op::jz ) && i.arg >= 0 && std::size_t( i.arg ) <= p.size() )
                i.arg = remap[ i.arg ];
        p = out;
    }
}

int main()
{
    using p = program;

    assert(( peephole( p{ { op::push, 2 }, { op::push, 3 }, { op::mul }, { op::push, 4 }, { op::add }, { op::print } } ) ==
             p{ { op::push, 10 }, { op::print } } ));
    assert(( peephole( p{ { op::load, 0 }, { op::push, 0 }, { op::add }, { op::push, 1 }, { op::mul }, { op::print } } ) ==
             p{ { op::load, 0 }, { op::print } } ));
    assert(( peephole( p{ { op::push, 1 }, { op::store, 0 }, { op::load, 0 }, { op::load, 0 }, { op::mul }, { op::print } } ) ==
             p{ { op::push, 1 }, { op::dup }, { op::dup }, { op::store, 0 }, { op::mul }, { op::print } } ));
    assert(( peephole( p{ { op::push, 5 }, { op::neg }, { op::push, 0 }, { op::not_ }, { op::sub }, { op::print } } ) ==
             p{ { op::push, -6 }, { op::print } } ));
    assert(( peephole( p{ { op::push, 1 }, { op::push, 0 }, { op::div }, { op::print } } ) ==
             p{ { op::push, 1 }, { op::push, 0 }, { op::div }, { op::print } } ));
    assert(( peephole( p{ { op::load, 3 }, { op::pop }, { op::dup }, { op::pop }, { op::swap }, { op::swap } } ) == p{} ));

    /* constant conditions: the jump becomes unconditional, and targets shift */
    assert(( peephole( p{ { op::push, 0 }, { op::jz, 4 }, { op::push, 7 }, { op::jump, 5 }, { op::push, 8 }, { op::print } } ) ==
             p{ { op::jump, 3 }, { op::push, 7 }, { op::jump, 4 }, { op::push, 8 }, { op::print } } ));
    assert(( peephole( p{ { op::push, 1 }, { op::jz, 3 }, { op::jump, 3 }, { op::push, 2 }, { op::print } } ) ==
             p{ { op::push, 2 }, { op::print } } ));

    /* a jump into the middle of a window blocks the rewrite */
    p blocked{ { op::load, 0 }, { op::jz, 4 }, { op::push, 1 }, { op::jump, 5 },
               { op::push, 2 }, { op::push, 3 }, { op::add }, { op::print } };
    blocked[ 3 ].arg = 5;
    assert( peephole( blocked ) == blocked );

    /* the optimized program behaves the same */
    p loop{ { op::push, 3 }, { op::store, 0 }, { op::load, 0 }, { op::jz, 13 },
            { op::load, 0 }, { op::push, 0 }, { op::add }, { op::print },
            { op::load, 0 }, { op::push, 1 }, { op::sub }, { op::store, 0 }, { op::jump, 2 },
            { op::push, 2 }, { op::push, 2 }, { op::mul }, { op::print } };
    auto opt = peephole( loop );
    assert( opt.size() < loop.size() );
    assert( execute( opt ) == execute( loop ) );
    assert(( execute( opt ) == std::vector< long >{ 3, 2, 1, 4 } ));

    return 0;
}
