#include <cassert>
#include <optional>
#include <string_view>

/* Write a function ‹eval_rpn› which evaluates an expression in
 * reverse Polish notation (see ‹02/p2_rpn›). The tokens are
 * separated by one or more spaces; a token is either an integer
 * (possibly with a leading ‹-›, like ‹-3›), or one of the operators
 * ‹+›, ‹-›, ‹*› and ‹/›.
 *
 * Use a stack: a number is pushed; an operator pops two values (the
 * «right» operand is on top), and pushes the result. At the end, the
 * stack must contain exactly one value, which is the result. Return
 * ‹std::nullopt› if an operator does not find two operands, if the
 * stack does not end up with exactly one value, if a token is
 * invalid, or on division by zero. */

// std::optional< long > eval_rpn( std::string_view s );

int main()
{
    assert( eval_rpn( "42" ) == 42 );
    assert( eval_rpn( "1 2 +" ) == 3 );
    assert( eval_rpn( "5 1 2 + 4 * + 3 -" ) == 14 );
    assert( eval_rpn( "10 2 /" ) == 5 );
    assert( eval_rpn( "2 10 -" ) == -8 );
    assert( eval_rpn( "-3 -4 *" ) == 12 );
    assert( eval_rpn( "  1   2  + " ) == 3 );

    assert( !eval_rpn( "" ) );
    assert( !eval_rpn( "+" ) );
    assert( !eval_rpn( "1 +" ) );
    assert( !eval_rpn( "1 2" ) );
    assert( !eval_rpn( "1 0 /" ) );
    assert( !eval_rpn( "1 x +" ) );
    assert( !eval_rpn( "1 2 %" ) );

    return 0;
}
