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

/* A small state machine: we are either in normal code, or inside a
 * string literal. Comments are skipped in one go, so they don't need
 * a state of their own. */

std::string strip_comments( std::string_view src )
{
    std::string out;
    bool in_string = false;
    std::size_t i = 0;

    auto starts = [ & ]( std::string_view what )
    {
        return src.substr( i ).starts_with( what );
    };

    while ( i < src.size() )
    {
        if ( in_string )
        {
            if ( src[ i ] == '\\' && i + 1 < src.size() )
                out += src[ i++ ];      /* copy the backslash … */
            else if ( src[ i ] == '"' )
                in_string = false;
            out += src[ i++ ];          /* … and the escaped char */
        }
        else if ( starts( "//" ) )
        {
            auto nl = src.find( '\n', i );
            i = nl == src.npos ? src.size() : nl;
        }
        else if ( starts( "/*" ) )
        {
            auto end = src.find( "*/", i + 2 );
            i = end == src.npos ? src.size() : end + 2;
            out += ' ';
        }
        else
        {
            if ( src[ i ] == '"' )
                in_string = true;
            out += src[ i++ ];
        }
    }

    return out;
}

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
