#include <cassert>
#include <optional>
#include <string_view>
#include <algorithm>
#include <string>
#include <vector>

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

std::optional< long > eval_rpn( std::string_view s )
{
    std::vector< long > stack;
    std::size_t i = 0;

    while ( true )
    {
        while ( i < s.size() && s[ i ] == ' ' )
            ++ i;
        if ( i == s.size() )
            break;

        auto end = std::min( s.find( ' ', i ), s.size() );
        auto tok = s.substr( i, end - i );
        i = end;

        bool number = tok.find_first_not_of( "0123456789", tok[ 0 ] == '-' ) == tok.npos &&
                      tok != "-";
        if ( number )
        {
            stack.push_back( std::stol( std::string( tok ) ) );
            continue;
        }

        if ( tok.size() != 1 || std::string_view( "+-*/" ).find( tok[ 0 ] ) == tok.npos ||
             stack.size() < 2 )
            return std::nullopt;

        long r = stack.back(); stack.pop_back();
        long l = stack.back(); stack.pop_back();

        switch ( tok[ 0 ] )
        {
            case '+': stack.push_back( l + r ); break;
            case '-': stack.push_back( l - r ); break;
            case '*': stack.push_back( l * r ); break;
            case '/':
                if ( r == 0 )
                    return std::nullopt;
                stack.push_back( l / r );
        }
    }

    if ( stack.size() != 1 )
        return std::nullopt;
    return stack.back();
}

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
