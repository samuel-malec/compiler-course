#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <charconv>
#include <cmath>
#include <map>
#include <memory>
#include <variant>
#include <vector>

/* In ‹02/p3_json›, we wrote a JSON «validator». Now, we will build
 * the corresponding tree. Since JSON is a data format, not a
 * programming language, the tree directly represents the «values»:
 * each node is either ‹null›, a boolean, a number, a string, an array
 * of values or an object (a map from strings to values).
 *
 * Write a class ‹json› and a function ‹parse_json›, which returns
 * ‹std::nullopt› if the input is not valid JSON (the grammar is the
 * same as in ‹02/p3_json›), and the root of the tree otherwise. The
 * class ‹json› must provide these (‹const›) methods:
 *
 *  • ‹is_null›, ‹is_bool›, ‹is_number›, ‹is_string›, ‹is_array› and
 *    ‹is_object›, which tell the kind of the value,
 *  • ‹as_bool›, ‹as_number› (returning ‹double›) and ‹as_string›
 *    (returning the «decoded» string), which throw
 *    ‹std::invalid_argument› if the value is of a different kind,
 *  • ‹size›, which returns the number of elements of an array or an
 *    object (and throws ‹std::invalid_argument› for other kinds),
 *  • ‹operator[]› with a ‹std::size_t› argument (for arrays) and with
 *    a ‹std::string_view› argument (for objects), which throw
 *    ‹std::out_of_range› if the index is out of bounds or the key is
 *    missing, and ‹std::invalid_argument› if the value is of a
 *    different kind,
 *  • ‹dump›, which serializes the value back to a ‹std::string›, in a
 *    canonical form (described below).
 *
 * Details of the decoding:
 *
 *  • in an object, a key may appear more than once; the last value
 *    wins,
 *  • the escape ‹\uXXXX› stands for a UTF-16 code unit; a code point
 *    outside the BMP is written as a «surrogate pair» – a «high»
 *    surrogate (‹D800› to ‹DBFF›) immediately followed by a «low» one
 *    (‹DC00› to ‹DFFF›); the pair ⟦(h, l)⟧ encodes the code point
 *    ⟦0x10000 + (h - 0xD800) · 0x400 + (l - 0xDC00)⟧; a surrogate
 *    which is not a part of such a pair is an error; decoded code
 *    points are stored in UTF-8 (see ‹01/p2_strlit›).
 *
 * The canonical form of ‹dump›:
 *
 *  • no whitespace at all,
 *  • object members ordered by key (in the order given by ‹<› on
 *    ‹std::string›),
 *  • in strings, ‹"› and ‹\› are escaped with a backslash, the
 *    characters with codes 8, 9, 10, 12 and 13 are written as ‹\b›,
 *    ‹\t›, ‹\n›, ‹\f› and ‹\r›, the remaining characters below 32 as
 *    ‹\u00XX› (lowercase hex), and everything else (including
 *    non-ASCII bytes) is written as is,
 *  • a number which is integral and smaller than ⟦10¹⁵⟧ in absolute
 *    value is written as an integer (no decimal point; negative zero
 *    is written as ‹0›); other numbers are written in the shortest
 *    form which reads back as the same ‹double›, as produced by
 *    ‹std::to_chars› without a format argument. */

// std::optional< json > parse_json( std::string_view s );

/* Arrays and objects contain ‹json› values, so they must be held
 * indirectly (the type is incomplete inside its own definition). The
 * tree is immutable once built, so sharing the containers through
 * ‹shared_ptr› makes ‹json› cheap to copy. */

class json
{
public:
    using array = std::vector< json >;
    using object = std::map< std::string, json >;

private:
    std::variant< std::monostate, bool, double, std::string,
                  std::shared_ptr< const array >,
                  std::shared_ptr< const object > > _v;

    template< typename T >
    const T &get() const
    {
        if ( auto p = std::get_if< T >( &_v ) )
            return *p;
        throw std::invalid_argument( "wrong kind of json value" );
    }

public:
    json() = default;
    explicit json( bool b ) : _v( b ) {}
    explicit json( double d ) : _v( d ) {}
    explicit json( std::string s ) : _v( std::move( s ) ) {}
    explicit json( array a ) : _v( std::make_shared< const array >( std::move( a ) ) ) {}
    explicit json( object o ) : _v( std::make_shared< const object >( std::move( o ) ) ) {}

    bool is_null()   const { return _v.index() == 0; }
    bool is_bool()   const { return _v.index() == 1; }
    bool is_number() const { return _v.index() == 2; }
    bool is_string() const { return _v.index() == 3; }
    bool is_array()  const { return _v.index() == 4; }
    bool is_object() const { return _v.index() == 5; }

    bool as_bool() const { return get< bool >(); }
    double as_number() const { return get< double >(); }
    const std::string &as_string() const { return get< std::string >(); }

    std::size_t size() const
    {
        if ( is_array() )
            return get< std::shared_ptr< const array > >()->size();
        return get< std::shared_ptr< const object > >()->size();
    }

    const json &operator[]( std::size_t i ) const
    {
        return get< std::shared_ptr< const array > >()->at( i );
    }

    const json &operator[]( std::string_view key ) const
    {
        const auto &obj = *get< std::shared_ptr< const object > >();
        auto it = obj.find( std::string( key ) );
        if ( it == obj.end() )
            throw std::out_of_range( "missing key" );
        return it->second;
    }

    std::string dump() const;
};

std::string dump_string( const std::string &s )
{
    std::string out = "\"";
    const char *hex = "0123456789abcdef";

    for ( unsigned char c : s )
        switch ( c )
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\f': out += "\\f"; break;
            case '\r': out += "\\r"; break;
            default:
                if ( c < 0x20 )
                    out += std::string( "\\u00" ) + hex[ c >> 4 ] + hex[ c & 15 ];
                else
                    out += c;
        }

    return out + "\"";
}

std::string dump_number( double d )
{
    if ( d == std::floor( d ) && std::fabs( d ) < 1e15 )
        return std::to_string( static_cast< long long >( d ) );

    char buf[ 64 ];
    auto [ end, _ ] = std::to_chars( buf, buf + sizeof buf, d );
    return std::string( buf, end );
}

std::string json::dump() const
{
    if ( is_null() )   return "null";
    if ( is_bool() )   return as_bool() ? "true" : "false";
    if ( is_number() ) return dump_number( as_number() );
    if ( is_string() ) return dump_string( as_string() );

    std::string out;

    if ( is_array() )
    {
        for ( const auto &x : *get< std::shared_ptr< const array > >() )
            out += ( out.empty() ? "" : "," ) + x.dump();
        return "[" + out + "]";
    }

    for ( const auto &[ k, x ] : *get< std::shared_ptr< const object > >() )
        out += ( out.empty() ? "" : "," ) + dump_string( k ) + ":" + x.dump();
    return "{" + out + "}";
}

/* The parser follows the validator from ‹02/p3_json›, but builds
 * values. Errors are signalled by an exception. */

struct json_error {};

struct json_parser
{
    std::string_view s;
    std::size_t pos = 0;

    char peek() const { return pos < s.size() ? s[ pos ] : '\0'; }
    bool at_end() const { return pos >= s.size(); }

    bool accept( char c )
    {
        if ( at_end() || s[ pos ] != c ) return false;
        ++ pos;
        return true;
    }

    void expect( char c ) { if ( !accept( c ) ) throw json_error(); }

    void ws()
    {
        while ( !at_end() && ( peek() == ' ' || peek() == '\t' ||
                               peek() == '\n' || peek() == '\r' ) )
            ++ pos;
    }

    void keyword( std::string_view kw )
    {
        if ( !s.substr( pos ).starts_with( kw ) )
            throw json_error();
        pos += kw.size();
    }

    json value()
    {
        ws();
        json v;

        switch ( peek() )
        {
            case '{': v = object(); break;
            case '[': v = array(); break;
            case '"': v = json( string() ); break;
            case 't': keyword( "true" ); v = json( true ); break;
            case 'f': keyword( "false" ); v = json( false ); break;
            case 'n': keyword( "null" ); break;
            default:  v = json( number() );
        }

        ws();
        return v;
    }

    json object()
    {
        json::object obj;
        expect( '{' );
        ws();

        if ( !accept( '}' ) )
        {
            do
            {
                ws();
                std::string key = string();
                ws();
                expect( ':' );
                obj.insert_or_assign( key, value() );
            } while ( accept( ',' ) );
            expect( '}' );
        }

        return json( std::move( obj ) );
    }

    json array()
    {
        json::array arr;
        expect( '[' );
        ws();

        if ( !accept( ']' ) )
        {
            do
                arr.push_back( value() );
            while ( accept( ',' ) );
            expect( ']' );
        }

        return json( std::move( arr ) );
    }

    unsigned hex4()
    {
        unsigned v = 0;
        for ( int i = 0; i < 4; ++ i )
        {
            char c = peek();
            ++ pos;
            int d = c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if ( d < 0 )
                throw json_error();
            v = v * 16 + d;
        }
        return v;
    }

    static void utf8( std::string &out, unsigned c )
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

    std::string string()
    {
        std::string out;
        expect( '"' );

        while ( !accept( '"' ) )
        {
            if ( at_end() || static_cast< unsigned char >( peek() ) < 0x20 )
                throw json_error();

            char c = s[ pos++ ];
            if ( c != '\\' )
            {
                out += c;
                continue;
            }

            switch ( char e = at_end() ? '\0' : s[ pos++ ] )
            {
                case '"': case '\\': case '/': out += e; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u':
                {
                    unsigned c1 = hex4();
                    if ( c1 >= 0xdc00 && c1 <= 0xdfff )
                        throw json_error(); /* lone low surrogate */
                    if ( c1 >= 0xd800 && c1 <= 0xdbff )
                    {
                        if ( !accept( '\\' ) || !accept( 'u' ) )
                            throw json_error();
                        unsigned c2 = hex4();
                        if ( c2 < 0xdc00 || c2 > 0xdfff )
                            throw json_error();
                        c1 = 0x10000 + ( c1 - 0xd800 ) * 0x400 + ( c2 - 0xdc00 );
                    }
                    utf8( out, c1 );
                    break;
                }
                default: throw json_error();
            }
        }

        return out;
    }

    double number()
    {
        std::size_t start = pos;
        auto digit = [ & ] { return peek() >= '0' && peek() <= '9'; };
        auto digits = [ & ]
        {
            if ( !digit() ) throw json_error();
            while ( digit() ) ++ pos;
        };

        accept( '-' );
        if ( !accept( '0' ) )
            digits();
        if ( accept( '.' ) )
            digits();
        if ( accept( 'e' ) || accept( 'E' ) )
        {
            if ( !accept( '+' ) )
                accept( '-' );
            digits();
        }

        double d = 0;
        std::from_chars( s.data() + start, s.data() + pos, d );
        return d;
    }
};

std::optional< json > parse_json( std::string_view s )
{
    json_parser p{ s };
    try
    {
        json v = p.value();
        if ( !p.at_end() )
            return std::nullopt;
        return v;
    }
    catch ( json_error ) { return std::nullopt; }
}

int main()
{
    auto j = parse_json( R"({ "b": [1, 2.5, "x"], "a": { "t": true, "n": null } })" );

    assert( j );
    assert( j->is_object() );
    assert( !j->is_array() );
    assert( j->size() == 2 );
    assert( ( *j )[ "b" ].is_array() );
    assert( ( *j )[ "b" ].size() == 3 );
    assert( ( *j )[ "b" ][ 1 ].as_number() == 2.5 );
    assert( ( *j )[ "b" ][ 2 ].as_string() == "x" );
    assert( ( *j )[ "a" ][ "t" ].as_bool() );
    assert( ( *j )[ "a" ][ "n" ].is_null() );
    assert( j->dump() == R"({"a":{"n":null,"t":true},"b":[1,2.5,"x"]})" );

    auto s = parse_json( R"("a\nbé😀\"\\\/")" );
    assert( s->as_string() == "a\nb\xc3\xa9\xf0\x9f\x98\x80\"\\/" );
    assert( s->dump() == "\"a\\nb\xc3\xa9\xf0\x9f\x98\x80\\\"\\\\/\"" );
    assert( parse_json( R"("\u0001\t")" )->dump() == R"("\u0001\t")" );

    assert( parse_json( "100" )->dump() == "100" );
    assert( parse_json( "1.0" )->dump() == "1" );
    assert( parse_json( "-0.0" )->dump() == "0" );
    assert( parse_json( "-0.5" )->dump() == "-0.5" );
    assert( parse_json( "1e20" )->dump() == "1e+20" );
    assert( parse_json( "[ ]" )->dump() == "[]" );
    assert( parse_json( " { } " )->dump() == "{}" );

    auto dup = parse_json( R"({"a": 1, "a": 2})" );
    assert( dup->size() == 1 );
    assert( ( *dup )[ "a" ].as_number() == 2 );

    auto throws = []( auto f, auto exc )
    {
        try { f(); }
        catch ( const decltype( exc ) & ) { return true; }
        return false;
    };

    std::out_of_range oor( "" );
    std::invalid_argument inv( "" );

    assert( throws( [ & ] { ( *j )[ "zzz" ]; }, oor ) );
    assert( throws( [ & ] { ( *j )[ "b" ][ 5 ]; }, oor ) );
    assert( throws( [ & ] { ( *j )[ "b" ].as_string(); }, inv ) );
    assert( throws( [ & ] { ( *j )[ 0 ]; }, inv ) );
    assert( throws( [ & ] { ( *j )[ "b" ][ 0 ].size(); }, inv ) );

    assert( !parse_json( "" ) );
    assert( !parse_json( "[1, 2,]" ) );
    assert( !parse_json( "{\"a\" 1}" ) );
    assert( !parse_json( "01" ) );
    assert( !parse_json( R"("\ud800")" ) );
    assert( !parse_json( R"("\ude00\ud83d")" ) );
    assert( !parse_json( R"("\ud83dx")" ) );
    assert( !parse_json( "[1] 2" ) );

    return 0;
}
