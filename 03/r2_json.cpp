#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

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

// class json { … };
// std::optional< json > parse_json( std::string_view s );

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
