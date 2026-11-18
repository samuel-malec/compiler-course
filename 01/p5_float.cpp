#include <cassert>
#include <optional>
#include <string_view>
#include <utility>

/* Floating-point literals are a nice example of a token where the
 * maximal munch rule requires a little care. Consider this grammar
 * (in EBNF; ‹[ x ]› means that ‹x› is optional):
 *
 *     digits   = digit, { digit } ;
 *     fraction = '.', digits ;
 *     exponent = ( 'e' | 'E' ), [ '+' | '-' ], digits ;
 *     float    = digits, fraction, [ exponent ]
 *              | digits, exponent
 *              | fraction, [ exponent ] ;
 *
 * Examples of valid literals: ‹1.5›, ‹1e10›, ‹.5›, ‹2.5E-3›.
 * Examples of things which are «not» valid literals: ‹1› (that's an
 * integer), ‹1.› (a fraction needs digits), ‹1e› or ‹1e+› (an
 * exponent needs digits).
 *
 * Write a function ‹lex_float› which finds the «longest prefix» of
 * the input which is a valid ‹float›, and returns its value (as a
 * ‹double›) and length. If no prefix of the input is a valid
 * ‹float›, return ‹std::nullopt›.
 *
 * The tricky part is that the lexer cannot always decide whether to
 * continue just by looking at the next character: in ‹1.5e+x›, the
 * characters ‹e+› «could» be the start of an exponent, but they turn
 * out not to be, and the longest valid prefix is ‹1.5›. A good way
 * to handle this is to walk through the input as far as possible,
 * remembering the «last position» at which the prefix read so far
 * was a valid literal.
 *
 * To compute the value, you may use ‹std::strtod› or
 * ‹std::from_chars› on the prefix you found – the point of this
 * exercise is to find the right prefix. */

// std::optional< std::pair< double, std::size_t > >
// lex_float( std::string_view input );

int main()
{
    using p = std::pair< double, std::size_t >;

    assert(( lex_float( "1.5" ) == p{ 1.5, 3 } ));
    assert(( lex_float( "1.5+x" ) == p{ 1.5, 3 } ));
    assert(( lex_float( "0.25e1" ) == p{ 2.5, 6 } ));
    assert(( lex_float( "2e3" ) == p{ 2000, 3 } ));
    assert(( lex_float( "2E+3;" ) == p{ 2000, 4 } ));
    assert(( lex_float( ".5" ) == p{ 0.5, 2 } ));
    assert(( lex_float( ".5e1" ) == p{ 5, 4 } ));
    assert(( lex_float( "1e-3" ) == p{ 0.001, 4 } ));
    assert(( lex_float( "007.5" ) == p{ 7.5, 5 } ));
    assert(( lex_float( "1.5.3" ) == p{ 1.5, 3 } ));

    assert(( lex_float( "1.5e" ) == p{ 1.5, 3 } ));
    assert(( lex_float( "1.5e+" ) == p{ 1.5, 3 } ));
    assert(( lex_float( "1.5e+x" ) == p{ 1.5, 3 } ));
    assert(( lex_float( "1.5ee" ) == p{ 1.5, 3 } ));

    assert( !lex_float( "" ) );
    assert( !lex_float( "1" ) );
    assert( !lex_float( "123;" ) );
    assert( !lex_float( "1." ) );
    assert( !lex_float( "1.e5" ) );
    assert( !lex_float( "1e" ) );
    assert( !lex_float( "1e+" ) );
    assert( !lex_float( "." ) );
    assert( !lex_float( ".e1" ) );
    assert( !lex_float( "e5" ) );
    assert( !lex_float( "-1.5" ) );

    return 0;
}
