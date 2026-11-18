#include <cassert>
#include <optional>
#include <string>
#include <vector>

/* Tree-walking is not the only way to interpret programs. In block 3,
 * we will compile programs into flat sequences of instructions for a
 * «virtual machine», which executes them one by one, using «jumps»
 * for control flow. As a preview, write an interpreter for a simple
 * «register machine». A program is a list of lines, each of which is
 * either empty, a «label» (a name followed by ‹:›, e.g. ‹loop:›), or
 * an «instruction»:
 *
 *  • ‹set r a› – store the value of ‹a› into register ‹r›,
 *  • ‹add r a b›, ‹sub r a b›, ‹mul r a b› – compute ‹a + b› (etc.)
 *    and store the result into ‹r›,
 *  • ‹jmp L› – continue with the first instruction after label ‹L›,
 *  • ‹jz a L›, ‹jnz a L› – jump to ‹L› if ‹a› is zero (non-zero),
 *    otherwise continue with the next instruction,
 *  • ‹out a› – append the value of ‹a› to the output,
 *  • ‹halt› – stop.
 *
 * Here, ‹r› is a register name (a non-empty sequence of lowercase
 * letters), and ‹a› and ‹b› are «operands»: either a register name or
 * an integer literal (with an optional ‹-›). All registers start at
 * 0. Words in a line are separated by one or more spaces (there may
 * also be spaces at the start or end of a line). The program stops
 * when it executes ‹halt›, or when it runs past the last line.
 *
 * Write a function ‹run( program, fuel )› which executes at most
 * ‹fuel› instructions (labels and empty lines do not count). If the
 * program stops before running out of fuel, the result has ‹halted›
 * set to ‹true›; otherwise, it is ‹false›. In both cases, ‹output›
 * contains the values printed so far.
 *
 * Before running anything, the program must be checked: if any line
 * is not a valid label or instruction (unknown instruction, wrong
 * number or kind of operands), if any jump refers to a label which
 * does not exist, or if a label is defined more than once, return
 * ‹std::nullopt›. */

struct result
{
    std::vector< long > output;
    bool halted;
};

// std::optional< result > run( const std::vector< std::string > &program, long fuel );

int main()
{
    using v = std::vector< long >;

    auto countdown = std::vector< std::string >
    {
        "  set n 3",
        "loop:",
        "  out n",
        "  sub n n 1",
        "  jnz n loop",
        "  out 100",
    };

    auto r = run( countdown, 1000 );
    assert( r && r->halted );
    assert(( r->output == v{ 3, 2, 1, 100 } ));

    r = run( countdown, 5 ); /* set, out, sub, jnz, out */
    assert( r && !r->halted );
    assert(( r->output == v{ 3, 2 } ));

    auto fib = std::vector< std::string >
    {
        "set a 0", "set b 1", "set i 10",
        "next:", "jz i done", "out a",
        "add t a b", "set a b", "set b t", "sub i i 1", "jmp next",
        "done:", "halt", "out -1",
    };

    r = run( fib, 1000 );
    assert( r && r->halted );
    assert(( r->output == v{ 0, 1, 1, 2, 3, 5, 8, 13, 21, 34 } ));

    auto forever = std::vector< std::string >{ "top:", "add x x 1", "jmp top" };
    r = run( forever, 1001 );
    assert( r && !r->halted && r->output.empty() );

    assert( run( {}, 0 )->halted );
    assert( run( { "", "   " }, 0 )->halted );
    assert( !run( { "halt" }, 0 )->halted );
    assert(( run( { "mul x -3 7", "out x" }, 10 )->output == v{ -21 } ));

    assert( !run( { "jmp nowhere" }, 10 ) );
    assert( !run( { "a:", "a:" }, 10 ) );
    assert( !run( { "set 1 2" }, 10 ) );
    assert( !run( { "add x 1" }, 10 ) );
    assert( !run( { "div x 1 2" }, 10 ) );
    assert( !run( { "out" }, 10 ) );
    assert( !run( { "out X" }, 10 ) );
    assert( !run( { "label: halt" }, 10 ) );
    assert( !run( { "halt", "jz x" }, 10 ) );

    return 0;
}
