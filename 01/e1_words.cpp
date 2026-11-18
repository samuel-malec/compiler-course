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
