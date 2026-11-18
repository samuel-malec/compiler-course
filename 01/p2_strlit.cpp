#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

/* String literals are the most complicated token in most languages,
 * because the lexer needs to «decode» them: the lexeme ‹"a\tb"›
 * (6 characters) stands for the 3-character string ‹a›, TAB, ‹b›.
 *
 * Write a function ‹string_literal› which takes the remaining input
 * of a lexer, starting with an opening double quote, and returns a
 * pair: the «decoded» value of the literal, and the number of input
 * characters it occupies (including both quotes). Whatever follows
 * the closing quote is not a part of the literal and must be
 * ignored. The following escape sequences must be supported:
 *
 *  • ‹\n›, ‹\t›, ‹\r› and ‹\0› (newline, tab, carriage return and
 *    the NUL character),
 *  • ‹\\›, ‹\"› and ‹\'› (backslash and quotes),
 *  • ‹\xHH›, where ‹HH› are «exactly» 2 hexadecimal digits: a single
 *    byte with the given value,
 *  • ‹\u{H…}›, where ‹H…› are 1 to 6 hexadecimal digits: the Unicode
 *    code point with the given value, «encoded in UTF-8».
 *
 * Hexadecimal digits can be upper or lower case. Return
 * ‹std::nullopt› if:
 *
 *  • the input does not start with ‹"›,
 *  • the literal is not terminated (there is no closing quote),
 *  • the literal contains a raw (unescaped) newline character,
 *  • it contains an unknown or malformed escape sequence,
 *  • a ‹\u{…}› escape denotes a value which is not a valid Unicode
 *    scalar value: that is, it is either larger than ‹0x10FFFF›, or
 *    it is a «surrogate» (between ‹0xD800› and ‹0xDFFF› inclusive).
 *
 * As a reminder, UTF-8 encodes a code point ⟦c⟧ like this:
 *
 * │ range of ⟦c⟧      │ bytes                               │
 * ├───────────────────│─────────────────────────────────────│
 * │ ‹0› – ‹7F›        │ ‹0xxxxxxx›                          │
 * │ ‹80› – ‹7FF›      │ ‹110xxxxx 10xxxxxx›                 │
 * │ ‹800› – ‹FFFF›    │ ‹1110xxxx 10xxxxxx 10xxxxxx›        │
 * │ ‹10000› – ‹10FFFF›│ ‹11110xxx 10xxxxxx 10xxxxxx 10xxxxxx›│
 *
 * where the bits ‹x› are the bits of ⟦c⟧, most significant first. */

// std::optional< std::pair< std::string, std::size_t > >
// string_literal( std::string_view input );

int main()
{
    using namespace std::literals;
    using p = std::pair< std::string, std::size_t >;

    assert(( string_literal( R"("")" ) == p{ "", 2 } ));
    assert(( string_literal( R"("abc")" ) == p{ "abc", 5 } ));
    assert(( string_literal( R"("abc" + "d")" ) == p{ "abc", 5 } ));
    assert(( string_literal( R"("a\tb")" ) == p{ "a\tb", 6 } ));
    assert(( string_literal( R"("\"\\\'")" ) == p{ "\"\\'", 8 } ));
    assert(( string_literal( R"("\n\r\0")" ) == p{ "\n\r\0"s, 8 } ));
    assert(( string_literal( R"("\x41\x6a")" ) == p{ "Aj", 10 } ));
    assert(( string_literal( R"("\xff")" ) == p{ "\xff", 6 } ));
    assert(( string_literal( R"("\u{41}")" ) == p{ "A", 8 } ));
    assert(( string_literal( R"("\u{e9}")" ) == p{ "\xc3\xa9", 8 } ));
    assert(( string_literal( R"("\u{20AC}")" ) == p{ "\xe2\x82\xac", 10 } ));
    assert(( string_literal( R"("\u{1F600}")" ) ==
             p{ "\xf0\x9f\x98\x80", 11 } ));
    assert(( string_literal( R"("\u{10FFFF}")" ) ==
             p{ "\xf4\x8f\xbf\xbf", 12 } ));

    assert( !string_literal( "abc" ) );
    assert( !string_literal( R"("abc)" ) );
    assert( !string_literal( R"("abc\")" ) );
    assert( !string_literal( "\"a\nb\"" ) );
    assert( !string_literal( R"("\q")" ) );
    assert( !string_literal( R"("\x4")" ) );
    assert( !string_literal( R"("\xg0")" ) );
    assert( !string_literal( R"("\u41")" ) );
    assert( !string_literal( R"("\u{}")" ) );
    assert( !string_literal( R"("\u{1234567}")" ) );
    assert( !string_literal( R"("\u{110000}")" ) );
    assert( !string_literal( R"("\u{D800}")" ) );
    assert( !string_literal( R"("\u{41")" ) );

    return 0;
}
