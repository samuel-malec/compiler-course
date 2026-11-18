#include <cassert>
#include <string_view>

/* Write a «validator» for JSON: a function ‹is_json› which decides
 * whether the input is a single valid JSON value, possibly surrounded
 * by whitespace. The grammar (simplified from RFC 8259, but complete
 * as far as the accepted language goes) is:
 *
 *     value   = ws, ( object | array | string | number |
 *                     'true' | 'false' | 'null' ), ws ;
 *     object  = '{', ws, [ member, { ',', ws, member } ], '}' ;
 *     member  = string, ws, ':', value ;
 *     array   = '[', ws, [ value, { ',', value } ], ']' ;
 *
 *     string  = '"', { char }, '"' ;
 *     char    = ? any character except '"', '\' and the control
 *                 characters 0x00 to 0x1f ?
 *             | '\', ( '"' | '\' | '/' | 'b' | 'f' | 'n' | 'r' | 't' )
 *             | '\u', hex, hex, hex, hex ;
 *
 *     number  = [ '-' ], int, [ frac ], [ exp ] ;
 *     int     = '0' | nonzero, { digit } ;
 *     frac    = '.', digit, { digit } ;
 *     exp     = ( 'e' | 'E' ), [ '+' | '-' ], digit, { digit } ;
 *
 *     ws      = { ' ' | '\t' | '\n' | '\r' } ;
 *
 * Note that the ‹ws› in ‹array› is redundant (‹value› starts with
 * ‹ws› anyway), but it is needed to allow ‹[ ]›. Do not try to
 * interpret the values in any way – just check the syntax. */

// bool is_json( std::string_view s );

int main()
{
    assert( is_json( "null" ) );
    assert( is_json( " true " ) );
    assert( is_json( "0" ) );
    assert( is_json( "-0.5e+10" ) );
    assert( is_json( "1E3" ) );
    assert( is_json( "\"\"" ) );
    assert( is_json( R"("a\"b\\c\/\b\f\n\r\t\u00e9\uABCD")" ) );
    assert( is_json( "[]" ) );
    assert( is_json( "[ ]" ) );
    assert( is_json( "{}" ) );
    assert( is_json( "{ }" ) );
    assert( is_json( "[1, 2, [3, []], {}]" ) );
    assert( is_json( R"({ "a": 1, "b": [true, false, null], "c": { "d": "e" } })" ) );
    assert( is_json( "\n\t{\r\n\"x\"\t:\n1}\n" ) );

    assert( !is_json( "" ) );
    assert( !is_json( "   " ) );
    assert( !is_json( "nul" ) );
    assert( !is_json( "True" ) );
    assert( !is_json( "01" ) );
    assert( !is_json( "-" ) );
    assert( !is_json( "+1" ) );
    assert( !is_json( "1." ) );
    assert( !is_json( ".5" ) );
    assert( !is_json( "1e" ) );
    assert( !is_json( "\"abc" ) );
    assert( !is_json( R"("\x41")" ) );
    assert( !is_json( R"("\u12g4")" ) );
    assert( !is_json( "\"a\tb\"" ) );
    assert( !is_json( "[1, 2,]" ) );
    assert( !is_json( "[1 2]" ) );
    assert( !is_json( "{\"a\" 1}" ) );
    assert( !is_json( "{\"a\": 1,}" ) );
    assert( !is_json( "{1: 2}" ) );
    assert( !is_json( "[1] [2]" ) );
    assert( !is_json( "{\"a\": }" ) );
    assert( !is_json( "[" ) );

    return 0;
}
