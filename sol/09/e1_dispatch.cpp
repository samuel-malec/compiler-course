#include <cassert>
#include <optional>
#include <string>
#include <vector>
#include <sstream>

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

std::optional< long > run( const std::vector< std::string > &code )
{
    std::vector< long > stack;
    auto pop = [ & ]( long &v ) { if ( stack.empty() ) return false; v = stack.back(); stack.pop_back(); return true; };

    for ( const auto &line : code )
    {
        std::istringstream in( line );
        std::string op;
        in >> op;
        long a, b;

        if ( op == "push" )
        {
            if ( !( in >> a ) ) return std::nullopt;
            stack.push_back( a );
        }
        else if ( op == "add" || op == "sub" || op == "mul" )
        {
            if ( !pop( b ) || !pop( a ) ) return std::nullopt;
            stack.push_back( op == "add" ? a + b : op == "sub" ? a - b : a * b );
        }
        else if ( op == "dup" )
        {
            if ( !pop( a ) ) return std::nullopt;
            stack.push_back( a ), stack.push_back( a );
        }
        else if ( op == "swap" )
        {
            if ( !pop( b ) || !pop( a ) ) return std::nullopt;
            stack.push_back( b ), stack.push_back( a );
        }
        else if ( op == "pop" )
        {
            if ( !pop( a ) ) return std::nullopt;
        }
        else
            return std::nullopt;
    }

    if ( stack.size() != 1 ) return std::nullopt;
    return stack[ 0 ];
}

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
