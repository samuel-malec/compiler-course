#include <cassert>
#include <string>
#include <string_view>

// Write a function ‹strip_comments›, which removes all comments from
// a piece of C-like source code (this specification uses line
// comments, since it needs to talk about the block comment markers):
//
//  • a «line comment» starts with ‹//› and extends up to (but «not»
//    including) the next newline character; remove it entirely,
//  • a «block comment» starts with ‹/*› and ends with the next ‹*/›;
//    replace it with a single space (so that ‹a/**/b› does not
//    become the single identifier ‹ab›); if there is no ‹*/›, the
//    comment extends to the end of input,
//  • comment markers inside «string literals» are «not» comments; a
//    string literal starts and ends with ‹"›, and can contain
//    escaped quotes (‹\"›) and escaped backslashes (‹\\›),
//  • likewise, a string quote inside a comment does not start a
//    string literal.
//
// Everything else must be copied to the output unchanged.

// std::string strip_comments( std::string_view src );

int main()
{
    assert( strip_comments( "a // b" ) == "a " );
    assert( strip_comments( "a // b\nc" ) == "a \nc" );
    assert( strip_comments( "a/**/b" ) == "a b" );
    assert( strip_comments( "a /* b\n c */ d" ) == "a   d" );
    assert( strip_comments( "a /* b" ) == "a  " );
    assert( strip_comments( "x = \"//\"; // y" ) == "x = \"//\"; " );
    assert( strip_comments( "\"a\\\"/*\" b" ) == "\"a\\\"/*\" b" );
    assert( strip_comments( "\"\\\\\" /**/" ) == "\"\\\\\"  " );
    assert( strip_comments( "/* \" */ x" ) == "  x" );
    assert( strip_comments( "a / b * c" ) == "a / b * c" );
    assert( strip_comments( "/*/ x */y" ) == " y" );

    return 0;
}
