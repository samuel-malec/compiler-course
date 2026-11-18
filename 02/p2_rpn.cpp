#include <cassert>
#include <optional>
#include <string>
#include <string_view>

/* «Reverse Polish notation» (RPN, or «postfix» notation) writes each
 * operator after its operands: ‹a + b * c› becomes ‹a b c * +›.
 * It needs no parentheses and no precedence rules, and it can be
 * evaluated with a simple stack – which is why stack-based virtual
 * machines (see block 3) execute code that looks just like this.
 *
 * Write a function ‹to_rpn› which translates an infix expression to
 * RPN. The input consists of:
 *
 *  • operands: identifiers (sequences of lowercase letters) and
 *    numbers (sequences of digits),
 *  • the binary operators ‹+›, ‹-›, ‹*›, ‹/› and ‹^›,
 *  • parentheses,
 *  • spaces, which separate tokens and are otherwise ignored.
 *
 * There are 3 levels of precedence: ‹+ -› (lowest), ‹* /›, and ‹^›
 * (highest). The operator ‹^› is right-associative, the others are
 * left-associative. There are no unary operators. The output must
 * list the tokens in postfix order, separated by single spaces. If
 * the input is not a valid expression, return ‹std::nullopt›.
 *
 * You can either write a recursive descent parser (or a Pratt
 * parser) which emits the tokens as it goes, or look up Dijkstra's
 * «shunting-yard» algorithm, which does the job with an explicit
 * stack of operators. */

// std::optional< std::string > to_rpn( std::string_view infix );

int main()
{
    assert( to_rpn( "a" ) == "a" );
    assert( to_rpn( "12 + 3" ) == "12 3 +" );
    assert( to_rpn( "a + b * c" ) == "a b c * +" );
    assert( to_rpn( "(a + b) * c" ) == "a b + c *" );
    assert( to_rpn( "a - b - c" ) == "a b - c -" );
    assert( to_rpn( "a - (b - c)" ) == "a b c - -" );
    assert( to_rpn( "a ^ b ^ c" ) == "a b c ^ ^" );
    assert( to_rpn( "(a ^ b) ^ c" ) == "a b ^ c ^" );
    assert( to_rpn( "a * b ^ c ^ d / e" ) == "a b c d ^ ^ * e /" );
    assert( to_rpn( "((x))" ) == "x" );
    assert( to_rpn( "a*b+c*d" ) == "a b * c d * +" );
    assert( to_rpn( "x / (y + z) ^ 2" ) == "x y z + 2 ^ /" );

    assert( !to_rpn( "" ) );
    assert( !to_rpn( "a +" ) );
    assert( !to_rpn( "+ a" ) );
    assert( !to_rpn( "(a" ) );
    assert( !to_rpn( "a)" ) );
    assert( !to_rpn( "a b" ) );
    assert( !to_rpn( "()" ) );
    assert( !to_rpn( "a + * b" ) );
    assert( !to_rpn( "a1" ) );

    return 0;
}
