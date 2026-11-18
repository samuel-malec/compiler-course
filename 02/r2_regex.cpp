#include <cassert>
#include <optional>
#include <string_view>

/* Regular expressions are a small language of their own, so they
 * need a parser too. Write a function ‹match( regex, text )› which
 * parses the regular expression and decides whether it matches the
 * «entire» ‹text›. The syntax of regular expressions is:
 *
 *     alt    = concat, { '|', concat } ;
 *     concat = { repeat } ;
 *     repeat = atom, { '*' | '+' | '?' } ;
 *     atom   = char | '.' | '(', alt, ')' | '\', any ;
 *
 * where ‹char› is any character other than ‹| * + ? ( ) . \›, and
 * ‹any› is any character at all. The meaning is as usual: ‹.›
 * matches any single character, ‹\c› matches ‹c› literally, ‹x*›
 * matches zero or more repetitions of ‹x›, ‹x+› one or more and ‹x?›
 * zero or one; ‹x|y› matches either ‹x› or ‹y›; concatenation binds
 * tighter than ‹|›, and the postfix operators bind tighter than
 * concatenation. Notice that ‹concat› can be empty, so ‹a|› and ‹()›
 * are valid (the empty alternative matches the empty string).
 *
 * If the regular expression is not syntactically valid, return
 * ‹std::nullopt›.
 *
 * The suggested approach is to first parse the expression into a
 * tree, and then match the tree against the text using backtracking.
 * A convenient formulation of the matcher is a function which takes
 * a node, a position in the text and a «continuation» (a function
 * which tries to match the rest), and returns whether the node,
 * followed by the continuation, matches. Watch out for repetition of
 * sub-expressions which can match the empty string (like ‹(a*)*›) –
 * a naive implementation will loop forever. The tests are small
 * enough that exponential worst-case behaviour is not a problem
 * here (we will fix that in the assignment set). */

// std::optional< bool > match( std::string_view regex, std::string_view text );

int main()
{
    assert( match( "", "" ) == true );
    assert( match( "", "a" ) == false );
    assert( match( "abc", "abc" ) == true );
    assert( match( "abc", "ab" ) == false );
    assert( match( "a.c", "axc" ) == true );
    assert( match( "a*", "" ) == true );
    assert( match( "a*", "aaaa" ) == true );
    assert( match( "a+", "" ) == false );
    assert( match( "a+b", "aaab" ) == true );
    assert( match( "colou?r", "color" ) == true );
    assert( match( "colou?r", "colour" ) == true );
    assert( match( "cat|dog", "dog" ) == true );
    assert( match( "cat|dog", "cats" ) == false );
    assert( match( "(ab)*c", "ababc" ) == true );
    assert( match( "(ab)*c", "abac" ) == false );
    assert( match( "a(b|c)*d", "abccbd" ) == true );
    assert( match( "(a|ab)(c|bcd)(d*)", "abcd" ) == true );
    assert( match( "a|", "" ) == true );
    assert( match( "()", "" ) == true );
    assert( match( "(a*)*b", "aaab" ) == true );
    assert( match( "(a*)*b", "aaaaaaaaaa" ) == false );
    assert( match( "(a|b*)*c", "abbac" ) == true );
    assert( match( "a**", "aa" ) == true );
    assert( match( "a?+", "aa" ) == true );
    assert( match( "\\.\\*", ".*" ) == true );
    assert( match( "\\.", "x" ) == false );
    assert( match( ".*x.*", "abxcd" ) == true );

    assert( !match( "(", "" ) );
    assert( !match( "a)", "" ) );
    assert( !match( "*a", "" ) );
    assert( !match( "a|*", "" ) );
    assert( !match( "(*)", "" ) );
    assert( !match( "a\\", "" ) );

    return 0;
}
