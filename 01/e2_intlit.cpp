#include <cassert>
#include <optional>
#include <string_view>

/* Write a function ‹int_literal› which decodes an integer literal.
 * The «entire» input string must be a single literal, in one of
 * these forms:
 *
 *     decimal = '0' | nonzero, { [ '_' ], digit } ;
 *     hex     = '0x', hexdigit, { [ '_' ], hexdigit } ;
 *     binary  = '0b', bindigit, { [ '_' ], bindigit } ;
 *     octal   = '0o', octdigit, { [ '_' ], octdigit } ;
 *
 * In words: digits can be separated by single underscores (for
 * readability, e.g. ‹1_000_000›), but an underscore cannot be the
 * first or the last character of the digit sequence, and two
 * underscores cannot follow each other. A decimal literal cannot
 * start with ‹0› unless it «is» ‹0›. Hexadecimal digits can be
 * either upper or lower case, the prefixes must be lower case.
 *
 * If the input is not a valid literal, or if its value does not fit
 * into a ‹long›, return ‹std::nullopt›. */

// std::optional< long > int_literal( std::string_view s );

int main()
{
    assert( int_literal( "0" ) == 0 );
    assert( int_literal( "42" ) == 42 );
    assert( int_literal( "1_000_000" ) == 1000000 );
    assert( int_literal( "0x2a" ) == 42 );
    assert( int_literal( "0xFF_ff" ) == 0xffff );
    assert( int_literal( "0b1010" ) == 10 );
    assert( int_literal( "0o777" ) == 511 );
    assert( int_literal( "9223372036854775807" ) == 9223372036854775807 );

    assert( !int_literal( "" ) );
    assert( !int_literal( "007" ) );
    assert( !int_literal( "0x" ) );
    assert( !int_literal( "0x_1" ) );
    assert( !int_literal( "1__0" ) );
    assert( !int_literal( "10_" ) );
    assert( !int_literal( "_10" ) );
    assert( !int_literal( "0b102" ) );
    assert( !int_literal( "0o8" ) );
    assert( !int_literal( "0X10" ) );
    assert( !int_literal( "12a" ) );
    assert( !int_literal( "9223372036854775808" ) );
    assert( !int_literal( "0x1_0000_0000_0000_0000" ) );

    return 0;
}
