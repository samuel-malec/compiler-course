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
