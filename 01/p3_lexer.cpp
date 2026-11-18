#include <cassert>
#include <string>
#include <string_view>
#include <vector>

/* In this exercise, you will write a complete lexer for a small
 * language (a somewhat bigger cousin of ‹tiny› from ‹d2_scanner›).
 * The kinds of tokens are given by the following enumeration: */

enum class kind
{
    identifier, keyword, integer, string, symbol, error, eof
};

/* Each token remembers its kind, its «text» (see below) and the line
 * and column of its first character, both numbered from 1. Columns
 * are counted in bytes. */

struct token
{
    kind k;
    std::string text;
    int line, column;

    bool operator==( const token & ) const = default;
};

/* The tokens are described as follows:
 *
 *  • «identifier» – a letter or ‹_›, followed by any number of
 *    letters, digits and ‹_›; the text is the lexeme,
 *  • «keyword» – one of ‹let›, ‹in›, ‹if›, ‹then›, ‹else›, ‹fun›,
 *    ‹true›, ‹false›, ‹and›, ‹or› and ‹not›; the text is the lexeme,
 *  • «integer» – a non-empty sequence of decimal digits; the text is
 *    the lexeme (do not decode the value),
 *  • «string» – a string literal enclosed in ‹"›, with escape
 *    sequences ‹\n›, ‹\"› and ‹\\›; the text is the «decoded»
 *    content (without the quotes),
 *  • «symbol» – one of ‹( ) , ; + - * / % = == != < <= > >= ->›;
 *    the text is the lexeme,
 *  • «eof» – the last token, at the position just past the end of
 *    the input; the text is empty.
 *
 * Whitespace (spaces, tabs and newlines) and comments (from ‹#› to
 * the end of the line) separate tokens. A string literal «may»
 * contain raw newlines (and the line counter must account for
 * them).
 *
 * Errors are reported using «error» tokens:
 *
 *  • a character which cannot start any token (including a lone ‹!›)
 *    gives an error token whose text is that character; lexing then
 *    continues with the next character,
 *  • an unknown escape sequence in a string gives an error token
 *    with the text ‹bad escape› at the position of the backslash; the
 *    lexer then skips to the end of the string literal (the next ‹"›
 *    that is not escaped) and continues after it,
 *  • a string literal without a closing quote gives an error token
 *    with the text ‹unterminated string› at the position of the
 *    opening quote; it is followed directly by ‹eof›.
 *
 * Write a function ‹lex› which returns the list of all tokens,
 * ending with the (single) ‹eof› token. */

// std::vector< token > lex( std::string_view src );

int main()
{
    using enum kind;
    using v = std::vector< token >;

    assert(( lex( "" ) == v{ { eof, "", 1, 1 } } ));
    assert(( lex( "  \n " ) == v{ { eof, "", 2, 2 } } ));

    assert(( lex( "let x = 42 in x" ) ==
             v{ { keyword, "let", 1, 1 }, { identifier, "x", 1, 5 },
                { symbol, "=", 1, 7 }, { integer, "42", 1, 9 },
                { keyword, "in", 1, 12 }, { identifier, "x", 1, 15 },
                { eof, "", 1, 16 } } ));

    assert(( lex( "fun f(a,b) -> a>=b" ) ==
             v{ { keyword, "fun", 1, 1 }, { identifier, "f", 1, 5 },
                { symbol, "(", 1, 6 }, { identifier, "a", 1, 7 },
                { symbol, ",", 1, 8 }, { identifier, "b", 1, 9 },
                { symbol, ")", 1, 10 }, { symbol, "->", 1, 12 },
                { identifier, "a", 1, 15 }, { symbol, ">=", 1, 16 },
                { identifier, "b", 1, 18 }, { eof, "", 1, 19 } } ));

    assert(( lex( "a # comment\n  -1 # another" ) ==
             v{ { identifier, "a", 1, 1 }, { symbol, "-", 2, 3 },
                { integer, "1", 2, 4 }, { eof, "", 2, 15 } } ));

    assert(( lex( R"("a\"b\\c\n")" ) ==
             v{ { string, "a\"b\\c\n", 1, 1 }, { eof, "", 1, 12 } } ));

    assert(( lex( "\"two\nlines\" x" ) ==
             v{ { string, "two\nlines", 1, 1 }, { identifier, "x", 2, 8 },
                { eof, "", 2, 9 } } ));

    assert(( lex( "a ! b != c" ) ==
             v{ { identifier, "a", 1, 1 }, { error, "!", 1, 3 },
                { identifier, "b", 1, 5 }, { symbol, "!=", 1, 7 },
                { identifier, "c", 1, 10 }, { eof, "", 1, 11 } } ));

    assert(( lex( "iffy if1 if" ) ==
             v{ { identifier, "iffy", 1, 1 }, { identifier, "if1", 1, 6 },
                { keyword, "if", 1, 10 }, { eof, "", 1, 12 } } ));

    assert(( lex( R"(x "a\qb\"c" y)" ) ==
             v{ { identifier, "x", 1, 1 }, { error, "bad escape", 1, 5 },
                { identifier, "y", 1, 13 }, { eof, "", 1, 14 } } ));

    assert(( lex( "1 \"abc\n" ) ==
             v{ { integer, "1", 1, 1 }, { error, "unterminated string", 1, 3 },
                { eof, "", 2, 1 } } ));

    assert(( lex( "12ab" ) ==
             v{ { integer, "12", 1, 1 }, { identifier, "ab", 1, 3 },
                { eof, "", 1, 5 } } ));

    return 0;
}
