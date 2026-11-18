#include <cassert>
#include <optional>
#include <string_view>
#include <limits>

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

/* Value of a digit in the given base, or -1 if ‹c› is not a valid
 * digit in that base. */

int digit_value( char c, int base )
{
    int v = -1;

    if ( c >= '0' && c <= '9' ) v = c - '0';
    if ( c >= 'a' && c <= 'f' ) v = c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' ) v = c - 'A' + 10;

    return v < base ? v : -1;
}

std::optional< long > int_literal( std::string_view s )
{
    int base = 10;

    if ( s.size() >= 2 && s[ 0 ] == '0' )
    {
        switch ( s[ 1 ] )
        {
            case 'x': base = 16; break;
            case 'b': base = 2;  break;
            case 'o': base = 8;  break;
            default: return std::nullopt; /* ‹007› and friends */
        }
        s.remove_prefix( 2 );
    }

    if ( s.empty() || s.front() == '_' || s.back() == '_' )
        return std::nullopt;

    long value = 0;
    const long max = std::numeric_limits< long >::max();

    for ( std::size_t i = 0; i < s.size(); ++ i )
    {
        if ( s[ i ] == '_' )
        {
            if ( s[ i + 1 ] == '_' ) /* safe: ‹s› does not end in ‹_› */
                return std::nullopt;
            continue;
        }

        int d = digit_value( s[ i ], base );
        if ( d < 0 || value > ( max - d ) / base )
            return std::nullopt;

        value = value * base + d;
    }

    return value;
}

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
