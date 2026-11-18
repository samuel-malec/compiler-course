#include <cassert>
#include <optional>
#include <string_view>

/* Write a calculator for integer expressions, using recursive
 * descent. The grammar is:
 *
 *     expr   = term, { ( '+' | '-' ), term } ;
 *     term   = unary, { ( '*' | '/' | '%' ), unary } ;
 *     unary  = '-', unary | power ;
 *     power  = atom, [ '^', unary ] ;
 *     atom   = number | '(', expr, ')' ;
 *     number = digit, { digit } ;
 *
 * Spaces may appear between any two tokens. Pay attention to these
 * consequences of the grammar:
 *
 *  • ‹^› (exponentiation) binds tighter than unary minus on its left,
 *    so ‹-2 ^ 2› is ‹-(2 ^ 2) = -4›,
 *  • but the exponent is a ‹unary›, so ‹2 ^ -1› is syntactically
 *    valid (it means ‹2 ^ (-1)›),
 *  • ‹^› is right-associative: ‹2 ^ 3 ^ 2 = 2 ^ 9 = 512›,
 *  • the other binary operators are left-associative.
 *
 * Use ‹long› for all values. The operators ‹/› and ‹%› behave like
 * they do in C++ (they truncate towards zero). ‹0 ^ 0› is 1. Return
 * ‹std::nullopt› if the input does not match the grammar, if it
 * divides by zero (using either ‹/› or ‹%›), or if an exponent is
 * negative. You don't need to worry about overflow. */

// std::optional< long > calc( std::string_view expr );

int main()
{
    assert( calc( "42" ) == 42 );
    assert( calc( " 42 " ) == 42 );
    assert( calc( "1 + 2 * 3" ) == 7 );
    assert( calc( "(1+2)*3" ) == 9 );
    assert( calc( "10 - 4 - 3" ) == 3 );
    assert( calc( "100 / 10 / 5" ) == 2 );
    assert( calc( "7 % 4 * 2" ) == 6 );
    assert( calc( "-7 / 2" ) == -3 );
    assert( calc( "-3 * -2" ) == 6 );
    assert( calc( "--3" ) == 3 );
    assert( calc( "2 ^ 10" ) == 1024 );
    assert( calc( "2^3^2" ) == 512 );
    assert( calc( "-2^2" ) == -4 );
    assert( calc( "(-2)^2" ) == 4 );
    assert( calc( "2 * 3 ^ 2" ) == 18 );
    assert( calc( "0 ^ 0" ) == 1 );
    assert( calc( "2 ^ -0" ) == 1 );

    assert( !calc( "" ) );
    assert( !calc( "1 +" ) );
    assert( !calc( "(1" ) );
    assert( !calc( "1)" ) );
    assert( !calc( "1 2" ) );
    assert( !calc( "2 ^ ^ 2" ) );
    assert( !calc( "1 / 0" ) );
    assert( !calc( "5 % (2 - 2)" ) );
    assert( !calc( "2 ^ -1" ) );
    assert( !calc( "2 ^ (1 - 2)" ) );
    assert( !calc( "1 / 0 * 0" ) );

    return 0;
}
