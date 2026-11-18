#include <cassert>
#include <string>
#include <string_view>
#include <vector>

/* Write a function ‹words› which splits the input string into
 * tokens, according to these rules:
 *
 *  • an «identifier» is a maximal sequence of letters, digits and
 *    underscores which starts with a letter or an underscore,
 *  • a «number» is a maximal sequence of digits (which is not a part
 *    of an identifier),
 *  • any other character which is not whitespace (space, tab or
 *    newline) is a token by itself,
 *  • whitespace separates tokens and is otherwise ignored.
 *
 * In particular, ‹12ab› is «two» tokens: the number ‹12› followed by
 * the identifier ‹ab›. You can assume that the input is ASCII (use
 * the character ranges ‹'a'› to ‹'z'› and so on to classify
 * characters). */

// std::vector< std::string > words( std::string_view text );

bool is_digit( char c ) { return c >= '0' && c <= '9'; }
bool is_ident_start( char c )
{
    return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_';
}
bool is_space( char c ) { return c == ' ' || c == '\t' || c == '\n'; }

std::vector< std::string > words( std::string_view text )
{
    std::vector< std::string > out;
    std::size_t i = 0;

    while ( i < text.size() )
    {
        std::size_t start = i;
        char c = text[ i++ ];

        if ( is_space( c ) )
            continue;

        if ( is_ident_start( c ) )
            while ( i < text.size() &&
                    ( is_ident_start( text[ i ] ) || is_digit( text[ i ] ) ) )
                ++ i;
        else if ( is_digit( c ) )
            while ( i < text.size() && is_digit( text[ i ] ) )
                ++ i;

        out.emplace_back( text.substr( start, i - start ) );
    }

    return out;
}

int main()
{
    using v = std::vector< std::string >;

    assert(( words( "let x = 12;" ) == v{ "let", "x", "=", "12", ";" } ));
    assert(( words( "a+b*c" ) == v{ "a", "+", "b", "*", "c" } ));
    assert(( words( "12ab" ) == v{ "12", "ab" } ));
    assert(( words( "ab12" ) == v{ "ab12" } ));
    assert(( words( "x_1 _y" ) == v{ "x_1", "_y" } ));
    assert(( words( "((" ) == v{ "(", "(" } ));
    assert(( words( "  \t\n " ) == v{} ));
    assert(( words( "" ) == v{} ));
    assert(( words( "f(x,\n  y)" ) == v{ "f", "(", "x", ",", "y", ")" } ));

    return 0;
}
