#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* In most languages, whitespace is insignificant: it separates
 * tokens, and that's it. Some languages (Python, Haskell, YAML) use
 * «indentation» to express structure instead of braces. Parsers for
 * such languages would be quite unpleasant to write if they had to
 * count spaces – instead, the lexer translates changes in
 * indentation into special tokens, ‹INDENT› and ‹DEDENT›, which the
 * parser can treat just like ‹{› and ‹}›.
 *
 * Write a function ‹layout› which does this translation. The input
 * is a sequence of lines, separated by ‹\n› (the last line may or
 * may not end with a newline). The output is a sequence of tokens,
 * computed as follows:
 *
 *  1. A line is «blank» if it contains only spaces, or if its first
 *     character which is not a space is ‹#›. Blank lines are ignored
 *     entirely (they do not produce any tokens).
 *  2. The «indentation» of a non-blank line is the number of spaces
 *     at its start. If the indentation contains a tab character,
 *     return ‹std::nullopt›.
 *  3. The lexer keeps a stack of indentation levels, which initially
 *     contains a single ‹0›. For each non-blank line:
 *     ◦ if its indentation is larger than the top of the stack, push
 *       it on the stack and emit ‹INDENT›,
 *     ◦ if it is smaller, pop all levels larger than the indentation,
 *       emitting one ‹DEDENT› for each; if the new top of the stack
 *       is not «equal» to the indentation, return ‹std::nullopt›
 *       (inconsistent dedent),
 *     ◦ then emit the «words» of the line (maximal sequences of
 *       characters other than space), up to the first word which
 *       starts with ‹#› (a comment), followed by ‹NEWLINE›.
 *  4. At the end of input, emit a ‹DEDENT› for each level on the
 *     stack other than the initial 0.
 *
 * Additionally, the first non-blank line must not be indented –
 * return ‹std::nullopt› if it is. */

// std::optional< std::vector< std::string > > layout( std::string_view src );

int main()
{
    using v = std::vector< std::string >;

    assert(( layout( "" ) == v{} ));
    assert(( layout( "a b c" ) == v{ "a", "b", "c", "NEWLINE" } ));

    assert(( layout( "if x:\n    y\n    z\nw\n" ) ==
             v{ "if", "x:", "NEWLINE",
                "INDENT", "y", "NEWLINE", "z", "NEWLINE",
                "DEDENT", "w", "NEWLINE" } ));

    assert(( layout( "a\n  b\n    c\nd" ) ==
             v{ "a", "NEWLINE", "INDENT", "b", "NEWLINE",
                "INDENT", "c", "NEWLINE", "DEDENT", "DEDENT",
                "d", "NEWLINE" } ));

    assert(( layout( "a\n  b\n    c\n  d\n" ) ==
             v{ "a", "NEWLINE", "INDENT", "b", "NEWLINE",
                "INDENT", "c", "NEWLINE", "DEDENT", "d", "NEWLINE",
                "DEDENT" } ));

    assert(( layout( "a\n  b" ) ==
             v{ "a", "NEWLINE", "INDENT", "b", "NEWLINE", "DEDENT" } ));

    assert(( layout( "a\n\n     \n   # hi\n  b\n" ) ==
             v{ "a", "NEWLINE", "INDENT", "b", "NEWLINE", "DEDENT" } ));

    assert(( layout( "a   b # c d\n" ) == v{ "a", "b", "NEWLINE" } ));

    assert( !layout( "a\n    b\n  c\n" ) );
    assert( !layout( "a\n\tb\n" ) );
    assert( !layout( "  a\n" ) );
    assert( !layout( "\n   a\n" ) );

    return 0;
}
