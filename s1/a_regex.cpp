#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

/* In ‹02/r2_regex›, we matched regular expressions by backtracking,
 * which takes exponential time in the worst case. Most popular regex
 * libraries (in Perl, Python, Java, JavaScript, PCRE, ‹std::regex›)
 * work that way, and real systems have been brought down by
 * carefully chosen patterns and inputs (‘ReDoS’). In this assignment,
 * you will implement a regex engine which «guarantees» running time
 * linear in the length of the input, using the approach of Ken
 * Thompson (1968), which is also used by RE2, Go and Rust:
 *
 *  1. «parse» the pattern into a syntax tree,
 *  2. «compile» the tree into a «nondeterministic finite automaton»
 *     (NFA), with states connected by character transitions and
 *     ‹ε› (empty) transitions,
 *  3. «simulate» the NFA on the input by keeping track of the «set»
 *     of all states the automaton could be in after reading each
 *     character – instead of trying the possibilities one at a time.
 *
 * Since the set of states has at most ⟦m⟧ elements (the size of the
 * NFA, which is linear in the size of the pattern), each character
 * of the input is processed in ⟦O(m)⟧ time, and the whole match takes
 * ⟦O(n · m)⟧. Look up ‘Regular Expression Matching Can Be Simple And
 * Fast’ by Russ Cox for a gentle introduction.
 *
 * The syntax of patterns:
 *
 *     alt    = concat, { '|', concat } ;
 *     concat = { repeat } ;
 *     repeat = atom, { '*' | '+' | '?' | count } ;
 *     count  = '{', int, '}' | '{', int, ',', '}' | '{', int, ',', int, '}' ;
 *     atom   = char | '.' | class | '(', alt, ')' | '\', any ;
 *     class  = '[', [ '^' ], member, { member }, ']' ;
 *     member = cchar, [ '-', cchar ] ;
 *
 * Here, ‹int› is a non-empty sequence of decimal digits, ‹char› is
 * any character except ‹| * + ? { } ( ) [ ] . \›, and ‹any› is any
 * character at all. Inside a class, ‹cchar› is either ‹\› followed by
 * any character (which stands for that character), or any character
 * other than ‹\› and ‹]›, except that a ‹]› which comes «first» in the
 * class (right after ‹[› or ‹[^›) is a literal ‹]›. A ‹-› which is the
 * first or the last character of a class is also literal.
 *
 * The meaning:
 *
 *  • ‹.› matches any single character (including a newline),
 *  • ‹[…]› matches any single character from the set; with ‹^›, any
 *    single character «not» in the set; ‹a-z› is the range of
 *    characters from ‹a› to ‹z› (by their ‹unsigned char› value)
 *    and it is an error if the end is smaller than the start,
 *  • ‹x*›, ‹x+› and ‹x?› are zero or more, one or more and zero or
 *    one repetitions of ‹x›,
 *  • ‹x{m}› is exactly ‹m› repetitions, ‹x{m,}› at least ‹m›, and
 *    ‹x{m,n}› between ‹m› and ‹n› (inclusive); it is an error if
 *    ‹m > n› or if ‹n› (or ‹m›, when there is no ‹n›) exceeds 1000,
 *  • ‹x|y›, concatenation, grouping and escapes as usual; the empty
 *    pattern (or an empty alternative) matches the empty string.
 *
 * Write a class ‹regex› with:
 *
 *  • a constructor which takes the pattern as a ‹std::string_view›
 *    and throws ‹std::invalid_argument› if it is not valid,
 *  • ‹match( text )› – a ‹const› method which returns ‹true› iff the
 *    «entire» ‹text› matches the pattern,
 *  • ‹search( text )› – a ‹const› method which finds the «leftmost»
 *    substring of ‹text› which matches the pattern and, among those
 *    starting at that position, the «longest» one; it returns the
 *    pair (start, length), or ‹std::nullopt› if there is no match.
 *
 * The method ‹match› must run in time ⟦O(n · m)⟧, where ⟦n⟧ is the
 * length of the text and ⟦m⟧ is the size of the pattern «after»
 * counted repetitions are expanded (‹a{3}› is the same as ‹aaa›). It
 * is fine for ‹search› to try each starting position separately
 * (⟦O(n² · m)⟧), but you can also get it down to ⟦O(n · m)⟧ with a
 * little more thought. */

// class regex;

int main()
{
    auto m = []( std::string_view p, std::string_view t ) { return regex( p ).match( t ); };

    assert( m( "", "" ) );
    assert( m( "abc", "abc" ) );
    assert( !m( "abc", "abcd" ) );
    assert( m( "a.c", "a\nc" ) );
    assert( m( "(ab|cd)*e", "abcdabe" ) );
    assert( m( "colou?r", "color" ) );
    assert( m( "[a-c]+x", "abcabcx" ) );
    assert( !m( "[a-c]+x", "abdx" ) );
    assert( m( "[^0-9]*", "abc" ) );
    assert( !m( "[^0-9]*", "ab1" ) );
    assert( m( "[]a]", "]" ) );
    assert( m( "[a-]", "-" ) );
    assert( m( "[\\]]", "]" ) );
    assert( m( "a{3}", "aaa" ) );
    assert( !m( "a{3}", "aa" ) );
    assert( m( "a{2,}", "aaaaa" ) );
    assert( m( "(ab){1,2}", "abab" ) );
    assert( !m( "(ab){1,2}", "ababab" ) );
    assert( m( "a|", "" ) );
    assert( m( "\\(\\)", "()" ) );

    auto throws = []( std::string_view p )
    {
        try { regex r( p ); }
        catch ( const std::invalid_argument & ) { return true; }
        return false;
    };

    assert( throws( "(" ) );
    assert( throws( ")" ) );
    assert( throws( "*" ) );
    assert( throws( "[" ) );
    assert( throws( "[]" ) );
    assert( throws( "[z-a]" ) );
    assert( throws( "a{2,1}" ) );
    assert( throws( "a{1001}" ) );
    assert( throws( "a{,3}" ) );
    assert( throws( "a\\" ) );
    assert( throws( "}" ) );

    using p = std::pair< std::size_t, std::size_t >;
    regex word( "[a-z]+" );
    assert( word.search( "123 hello world" ) == p( 4, 5 ) );
    assert( !word.search( "123 456" ) );
    assert( regex( "a*" ).search( "bbb" ) == p( 0, 0 ) );
    assert( regex( "ab|abcd" ).search( "xabcde" ) == p( 1, 4 ) );

    /* Pathological for backtracking: ‹(a?){n}a{n}› against ‹aⁿ›. */
    std::string pat, text( 30, 'a' );
    for ( int i = 0; i < 30; ++ i ) pat += "a?";
    pat += "a{30}";
    assert( m( pat, text ) );
    assert( !m( pat, text + "b" ) );
    assert( m( "(a*)*b|a*", std::string( 5000, 'a' ) ) );

    return 0;
}
