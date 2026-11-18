#include <cassert>
#include <optional>
#include <string>
#include <vector>

/* Write the core of a bytecode interpreter: a function ‹run› which
 * executes a program for a tiny stack machine and returns the value
 * left on the stack. The program is a list of instructions, as
 * strings:
 *
 *  • ‹push N› – push the integer ‹N› (possibly negative),
 *  • ‹add›, ‹sub›, ‹mul› – pop ‹b›, pop ‹a›, push ‹a + b› (etc.),
 *  • ‹dup› – push a copy of the top value,
 *  • ‹swap› – exchange the two top values,
 *  • ‹pop› – drop the top value.
 *
 * At the end, the stack must contain exactly one value, which is the
 * result. Return ‹std::nullopt› on a stack underflow, if the final stack
 * does not have exactly one value, or for an unknown instruction. */

// std::optional< long > run( const std::vector< std::string > &code );

int main()
{
    assert( run( { "push 42" } ) == 42 );
    assert( run( { "push 1", "push 2", "add", "push 3", "mul" } ) == 9 );
    assert( run( { "push 10", "push 3", "sub" } ) == 7 );
    assert( run( { "push 10", "push 3", "swap", "sub" } ) == -7 );
    assert( run( { "push -4", "dup", "mul" } ) == 16 );
    assert( run( { "push 1", "push 2", "pop" } ) == 1 );

    assert( !run( {} ) );
    assert( !run( { "add" } ) );
    assert( !run( { "push 1", "push 2" } ) );
    assert( !run( { "push 1", "swap" } ) );
    assert( !run( { "push 1", "div" } ) );

    return 0;
}
