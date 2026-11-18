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

struct json_validator
{
    std::string_view s;
    std::size_t pos = 0;

    char peek() const { return pos < s.size() ? s[ pos ] : '\0'; }
    bool at_end() const { return pos >= s.size(); }

    bool accept( char c )
    {
        if ( at_end() || s[ pos ] != c )
            return false;
        ++ pos;
        return true;
    }

    bool keyword( std::string_view kw )
    {
        if ( !s.substr( pos ).starts_with( kw ) )
            return false;
        pos += kw.size();
        return true;
    }

    static bool is_digit( char c ) { return c >= '0' && c <= '9'; }
    static bool is_hex( char c )
    {
        return is_digit( c ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' );
    }

    void ws()
    {
        while ( !at_end() && ( peek() == ' ' || peek() == '\t' ||
                               peek() == '\n' || peek() == '\r' ) )
            ++ pos;
    }

    bool digits()
    {
        if ( !is_digit( peek() ) )
            return false;
        while ( is_digit( peek() ) )
            ++ pos;
        return true;
    }

    bool value()
    {
        ws();
        bool ok;

        switch ( peek() )
        {
            case '{': ok = object(); break;
            case '[': ok = array(); break;
            case '"': ok = string(); break;
            case 't': ok = keyword( "true" ); break;
            case 'f': ok = keyword( "false" ); break;
            case 'n': ok = keyword( "null" ); break;
            default:  ok = number();
        }

        ws();
        return ok;
    }

    bool object()
    {
        accept( '{' );
        ws();
        if ( accept( '}' ) )
            return true;

        do
        {
            ws();
            if ( !string() )
                return false;
            ws();
            if ( !accept( ':' ) || !value() )
                return false;
        } while ( accept( ',' ) );

        return accept( '}' );
    }

    bool array()
    {
        accept( '[' );
        ws();
        if ( accept( ']' ) )
            return true;

        do
            if ( !value() )
                return false;
        while ( accept( ',' ) );

        return accept( ']' );
    }

    bool string()
    {
        if ( !accept( '"' ) )
            return false;

        while ( !accept( '"' ) )
        {
            if ( at_end() || static_cast< unsigned char >( peek() ) < 0x20 )
                return false;

            if ( !accept( '\\' ) )
            {
                ++ pos;
                continue;
            }

            char e = peek();
            ++ pos;

            if ( e == 'u' )
            {
                for ( int i = 0; i < 4; ++ i, ++ pos )
                    if ( !is_hex( peek() ) )
                        return false;
            }
            else if ( std::string_view( "\"\\/bfnrt" ).find( e ) == s.npos || e == '\0' )
                return false;
        }

        return true;
    }

    bool number()
    {
        accept( '-' );

        if ( !accept( '0' ) && !digits() )
            return false;

        if ( accept( '.' ) && !digits() )
            return false;

        if ( accept( 'e' ) || accept( 'E' ) )
        {
            if ( !accept( '+' ) )
                accept( '-' );
            if ( !digits() )
                return false;
        }

        return true;
    }
};

bool is_json( std::string_view s )
{
    json_validator v{ s };
    return v.value() && v.at_end();
}

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
