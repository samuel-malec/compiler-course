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

int hex_value( char c )
{
    if ( c >= '0' && c <= '9' ) return c - '0';
    if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
    return -1;
}

void put_utf8( std::string &out, unsigned c )
{
    auto byte = [ & ]( unsigned b ) { out += static_cast< char >( b ); };

    if ( c < 0x80 )
        byte( c );
    else if ( c < 0x800 )
        byte( 0xc0 | c >> 6 ), byte( 0x80 | ( c & 0x3f ) );
    else if ( c < 0x10000 )
        byte( 0xe0 | c >> 12 ), byte( 0x80 | ( c >> 6 & 0x3f ) ),
        byte( 0x80 | ( c & 0x3f ) );
    else
        byte( 0xf0 | c >> 18 ), byte( 0x80 | ( c >> 12 & 0x3f ) ),
        byte( 0x80 | ( c >> 6 & 0x3f ) ), byte( 0x80 | ( c & 0x3f ) );
}

std::optional< std::pair< std::string, std::size_t > >
string_literal( std::string_view in )
{
    if ( in.empty() || in[ 0 ] != '"' )
        return std::nullopt;

    std::string out;
    std::size_t i = 1;

    /* Returns the next character, or NUL past the end; the callers
     * check ‹i› against the size where it matters. */
    auto next = [ & ] { return i < in.size() ? in[ i++ ] : ( ++i, '\0' ); };

    while ( true )
    {
        if ( i >= in.size() )
            return std::nullopt;

        char c = next();

        if ( c == '"' )
            return std::pair{ out, i };
        if ( c == '\n' )
            return std::nullopt;
        if ( c != '\\' )
        {
            out += c;
            continue;
        }

        switch ( next() )
        {
            case 'n':  out += '\n'; break;
            case 't':  out += '\t'; break;
            case 'r':  out += '\r'; break;
            case '0':  out += '\0'; break;
            case '\\': out += '\\'; break;
            case '"':  out += '"';  break;
            case '\'': out += '\''; break;

            case 'x':
            {
                int hi = hex_value( next() ), lo = hex_value( next() );
                if ( hi < 0 || lo < 0 )
                    return std::nullopt;
                out += static_cast< char >( hi * 16 + lo );
                break;
            }

            case 'u':
            {
                if ( next() != '{' )
                    return std::nullopt;

                unsigned value = 0;
                int digits = 0;
                char d;

                while ( ( d = next() ) != '}' )
                {
                    int v = hex_value( d );
                    if ( v < 0 || ++ digits > 6 )
                        return std::nullopt;
                    value = value * 16 + v;
                }

                if ( digits == 0 || value > 0x10ffff ||
                     ( value >= 0xd800 && value <= 0xdfff ) )
                    return std::nullopt;

                put_utf8( out, value );
                break;
            }

            default:
                return std::nullopt;
        }
    }
}

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
