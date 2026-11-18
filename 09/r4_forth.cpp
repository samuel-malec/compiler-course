#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* «Forth» is a stack language whose implementations are tiny compilers:
 * a definition is compiled, word by word, into a list of references to
 * other words (‘threaded code’), and executing it just runs through
 * the list. Write an interpreter for this subset of Forth. The input is
 * a sequence of words separated by whitespace (case-sensitive):
 *
 *  • an integer (with an optional ‹-›) pushes itself,
 *  • ‹+ - * /› and ‹mod› – arithmetic on the two top values (‹/› and
 *    ‹mod› truncate as in C++; division by zero is an error),
 *  • ‹= <› – comparison, pushes 1 (true) or 0 (false),
 *  • ‹dup drop swap over› – ‹( a -- a a )›, ‹( a -- )›, ‹( a b -- b a )›,
 *    ‹( a b -- a b a )›,
 *  • ‹.› – pop a value and append it to the output,
 *  • ‹: NAME … ;› – define a new word ‹NAME› (any whitespace-free text
 *    that is not an integer); the body is «compiled» at definition
 *    time: every word in it must already be defined (or be an integer,
 *    ‹NAME› itself for recursion, or one of the control words below),
 *    and it refers to the definition which is current «at that time» –
 *    redefining a word later does not change words compiled before,
 *  • inside a definition only: ‹if … then›, ‹if … else … then› (‹if›
 *    pops a value and runs the first part if it is non-zero), and
 *    ‹begin … until› (runs the body, then pops a value, and repeats if
 *    it is zero); these can be nested.
 *
 * Words outside definitions are executed immediately. User words may
 * redefine built-in words (except ‹:› and ‹;›). Return the output, or
 * ‹std::nullopt› on any error: an unknown word, a stack underflow, a
 * malformed definition (missing ‹;›, missing name, unbalanced control
 * words, control words outside a definition), division by zero, or
 * more than a million executed words. */

// std::optional< std::vector< long > > forth( std::string_view program );

int main()
{
    using v = std::vector< long >;

    assert(( forth( "1 2 + ." ) == v{ 3 } ));
    assert(( forth( "10 3 - . 7 2 / . -7 2 mod ." ) == v{ 7, 3, -1 } ));
    assert(( forth( "1 2 swap . . 5 dup * . 1 2 over . . . 4 drop" ) == v{ 1, 2, 25, 1, 2, 1 } ));
    assert(( forth( ": square dup * ; 7 square ." ) == v{ 49 } ));
    assert(( forth( ": abs dup 0 < if 0 swap - then ; -5 abs . 5 abs ." ) == v{ 5, 5 } ));
    assert(( forth( ": sign dup 0 < if drop -1 else 0 = if 0 else 1 then then ; -3 sign . 0 sign . 9 sign ." ) ==
             v{ -1, 0, 1 } ));
    assert(( forth( ": countdown begin dup . 1 - dup 0 = until drop ; 3 countdown" ) == v{ 3, 2, 1 } ));
    assert(( forth( ": fact dup 1 < if drop 1 else dup 1 - fact * then ; 10 fact ." ) == v{ 3628800 } ));

    /* early binding: ‹bar› keeps using the old ‹foo› */
    assert(( forth( ": foo 1 ; : bar foo ; : foo 2 ; bar . foo ." ) == v{ 1, 2 } ));
    assert(( forth( ": + * ; 3 4 + ." ) == v{ 12 } ));

    assert( !forth( "1 +" ) );
    assert( !forth( "frob" ) );
    assert( !forth( ": broken frob ;" ) );
    assert( !forth( ": nosemi 1 2" ) );
    assert( !forth( ": ;" ) );
    assert( !forth( ": bad if 1 ;" ) );
    assert( !forth( "1 if 2 then" ) );
    assert( !forth( "1 0 /" ) );
    assert( !forth( ": loop begin 0 until ; loop" ) );

    return 0;
}
