#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* Many modern languages support «string interpolation»: the string
 * literal ‹"hello ${name}!"› contains an «expression» (here just the
 * variable ‹name›) whose value is spliced into the string. The
 * expression can be arbitrary, so it can contain other string
 * literals, which can contain further interpolations, and so on.
 *
 * Such a literal cannot be a single token: the lexer has to produce
 * tokens for the expression inside. This means that the lexer needs
 * to switch between two «modes» – lexing code and lexing the inside
 * of a string – and, since interpolations nest, it needs a «stack»
 * of modes. Another complication is that the expression can contain
 * braces of its own, and the lexer must not confuse a ‹}› that
 * closes such a brace with the ‹}› that ends the interpolation.
 *
 * Write a function ‹interp› which lexes the input into these tokens
 * (each represented by a string):
 *
 *  • in «code» mode:
 *    ◦ spaces are skipped,
 *    ◦ an identifier (a maximal sequence of lowercase letters) gives
 *      ‹id:NAME›, e.g. ‹id:x›,
 *    ◦ the characters ‹+›, ‹{› and ‹}› give the tokens ‹+›, ‹{› and
 *      ‹}›, except that a ‹}› which ends an interpolation gives the
 *      token ‹$}› instead (and switches back to string mode),
 *    ◦ ‹"› gives the token ‹"› and starts a string,
 *    ◦ any other character is an error,
 *  • in «string» mode:
 *    ◦ a maximal sequence of literal characters gives ‹s:TEXT›,
 *      where ‹TEXT› is decoded: the escapes ‹\"›, ‹\\› and ‹\$›
 *      stand for ‹"›, ‹\› and ‹$›; any other escape is an error;
 *      a ‹$› which is not followed by ‹{› is a literal character,
 *    ◦ ‹${› gives the token ‹${› and starts an interpolation (code
 *      mode),
 *    ◦ ‹"› gives the token ‹"› and ends the string.
 *
 * The input starts in code mode. Return ‹std::nullopt› on any error,
 * including: an unterminated string or interpolation, a ‹}› without
 * a matching ‹{› or ‹${›, or a ‹{› that is never closed. Notice that
 * ‹s:…› tokens are never empty: ‹""› gives just two ‹"› tokens. */

// std::optional< std::vector< std::string > > interp( std::string_view src );

/* The mode stack: the bottom (implicit) mode is code; each entry is
 * either a string, an interpolation (which is code mode, ended by a
 * ‹}›) or a brace (also code mode, ended by a ‹}›). */

enum class mode { string, interpolation, brace };

std::optional< std::vector< std::string > > interp( std::string_view src )
{
    std::vector< std::string > out;
    std::vector< mode > stack;
    std::string chunk;
    std::size_t i = 0;

    auto flush = [ & ]
    {
        if ( !chunk.empty() )
            out.push_back( "s:" + chunk );
        chunk.clear();
    };

    while ( i < src.size() )
    {
        char c = src[ i ];

        if ( !stack.empty() && stack.back() == mode::string )
        {
            if ( c == '"' )
            {
                flush();
                out.emplace_back( "\"" );
                stack.pop_back();
                ++ i;
            }
            else if ( c == '$' && i + 1 < src.size() && src[ i + 1 ] == '{' )
            {
                flush();
                out.emplace_back( "${" );
                stack.push_back( mode::interpolation );
                i += 2;
            }
            else if ( c == '\\' )
            {
                if ( i + 1 >= src.size() )
                    return std::nullopt;
                char e = src[ i + 1 ];
                if ( e != '"' && e != '\\' && e != '$' )
                    return std::nullopt;
                chunk += e;
                i += 2;
            }
            else
            {
                chunk += c;
                ++ i;
            }

            continue;
        }

        /* code mode */
        ++ i;

        if ( c == ' ' )
            continue;

        if ( c >= 'a' && c <= 'z' )
        {
            std::size_t start = i - 1;
            while ( i < src.size() && src[ i ] >= 'a' && src[ i ] <= 'z' )
                ++ i;
            out.push_back( "id:" + std::string( src.substr( start, i - start ) ) );
        }
        else if ( c == '+' )
            out.emplace_back( "+" );
        else if ( c == '"' )
        {
            out.emplace_back( "\"" );
            stack.push_back( mode::string );
        }
        else if ( c == '{' )
        {
            out.emplace_back( "{" );
            stack.push_back( mode::brace );
        }
        else if ( c == '}' )
        {
            if ( stack.empty() )
                return std::nullopt;
            out.emplace_back( stack.back() == mode::brace ? "}" : "$}" );
            stack.pop_back();
        }
        else
            return std::nullopt;
    }

    if ( !stack.empty() )
        return std::nullopt;

    return out;
}

int main()
{
    using v = std::vector< std::string >;

    assert(( interp( "" ) == v{} ));
    assert(( interp( "x + \"y\"" ) ==
             v{ "id:x", "+", "\"", "s:y", "\"" } ));
    assert(( interp( "\"\"" ) == v{ "\"", "\"" } ));
    assert(( interp( "\"a${x}b\"" ) ==
             v{ "\"", "s:a", "${", "id:x", "$}", "s:b", "\"" } ));
    assert(( interp( "\"${}\"" ) == v{ "\"", "${", "$}", "\"" } ));
    assert(( interp( "\"${ { x } }\"" ) ==
             v{ "\"", "${", "{", "id:x", "}", "$}", "\"" } ));
    assert(( interp( "\"a${ \"b${c}\" + d }\"" ) ==
             v{ "\"", "s:a", "${", "\"", "s:b", "${", "id:c", "$}",
                "\"", "+", "id:d", "$}", "\"" } ));
    assert(( interp( "\"cost: \\$${n}\"" ) ==
             v{ "\"", "s:cost: $", "${", "id:n", "$}", "\"" } ));
    assert(( interp( "\"a$b$\"" ) == v{ "\"", "s:a$b$", "\"" } ));
    assert(( interp( "\"\\\"q\\\\\"" ) == v{ "\"", "s:\"q\\", "\"" } ));
    assert(( interp( "{ \"}\" }" ) == v{ "{", "\"", "s:}", "\"", "}" } ));

    assert( !interp( "\"abc" ) );
    assert( !interp( "\"${x\"" ) );
    assert( !interp( "\"${x}" ) );
    assert( !interp( "x }" ) );
    assert( !interp( "{ x" ) );
    assert( !interp( "\"${ { x }\"" ) );
    assert( !interp( "\"\\n\"" ) );
    assert( !interp( "x # y" ) );
    assert( !interp( "X" ) );

    return 0;
}
