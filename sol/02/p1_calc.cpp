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

/* We evaluate while parsing. Errors (both syntax and arithmetic) are
 * signalled by throwing ‹calc_error›, which is caught at the top –
 * this keeps the parsing functions free of error plumbing. */

struct calc_error {};

struct calc_parser
{
    std::string_view s;
    std::size_t pos = 0;

    char peek()
    {
        while ( pos < s.size() && s[ pos ] == ' ' )
            ++ pos;
        return pos < s.size() ? s[ pos ] : '\0';
    }

    bool accept( char c )
    {
        if ( peek() != c )
            return false;
        ++ pos;
        return true;
    }

    long expr()
    {
        long v = term();
        while ( true )
            if      ( accept( '+' ) ) v += term();
            else if ( accept( '-' ) ) v -= term();
            else return v;
    }

    long term()
    {
        long v = unary();
        while ( true )
        {
            char op = peek();
            if ( op != '*' && op != '/' && op != '%' )
                return v;
            ++ pos;

            long r = unary();
            if ( op != '*' && r == 0 )
                throw calc_error();
            v = op == '*' ? v * r : op == '/' ? v / r : v % r;
        }
    }

    long unary()
    {
        if ( accept( '-' ) )
            return -unary();
        return power();
    }

    long power()
    {
        long base = atom();
        if ( !accept( '^' ) )
            return base;

        long exp = unary();
        if ( exp < 0 )
            throw calc_error();

        long r = 1;
        while ( exp-- > 0 )
            r *= base;
        return r;
    }

    long atom()
    {
        if ( accept( '(' ) )
        {
            long v = expr();
            if ( !accept( ')' ) )
                throw calc_error();
            return v;
        }

        char c = peek();
        if ( c < '0' || c > '9' )
            throw calc_error();

        long v = 0;
        while ( pos < s.size() && s[ pos ] >= '0' && s[ pos ] <= '9' )
            v = v * 10 + ( s[ pos++ ] - '0' );
        return v;
    }
};

std::optional< long > calc( std::string_view expr )
{
    calc_parser p{ expr };

    try
    {
        long v = p.expr();
        if ( p.peek() != '\0' )
            return std::nullopt;
        return v;
    }
    catch ( calc_error ) { return std::nullopt; }
}

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
