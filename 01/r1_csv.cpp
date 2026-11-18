#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* Lexers are not only useful for programming languages – any
 * structured text format needs one. In this exercise, we will write
 * a lexer (which is, in this case, also a parser) for CSV files, as
 * described by RFC 4180:
 *
 *  • a file is a sequence of «records», separated by line breaks
 *    (either ‹\n› or ‹\r\n›); a line break after the last record is
 *    optional (and does not start a new, empty record),
 *  • each record is a sequence of «fields», separated by commas,
 *  • a field is either «unquoted» – any sequence of characters other
 *    than ‹,›, ‹"›, ‹\r› and ‹\n› (possibly empty) – or «quoted» –
 *    enclosed in ‹"›; inside a quoted field, commas and line breaks
 *    are literal characters, and a quote is written as two quotes
 *    (‹""›).
 *
 * Write a function ‹parse_csv› which returns the records, each as a
 * vector of (decoded) fields. An empty input has no records. Return
 * ‹std::nullopt› if the input is malformed: that is, if a quoted
 * field is not terminated, if a closing quote is followed by
 * something other than a comma, a line break or the end of input, or
 * if an unquoted field contains a ‹"›, or a ‹\r› which is not a part
 * of ‹\r\n›.
 *
 * The cleanest solution is a «state machine»: a variable which says
 * where we are (at the start of a field, inside an unquoted field,
 * inside a quoted field, just after a quote inside a quoted field),
 * and a loop with a ‹switch› on that variable. */

// std::optional< std::vector< std::vector< std::string > > >
// parse_csv( std::string_view src );

int main()
{
    using rec = std::vector< std::string >;
    using file = std::vector< rec >;

    assert(( parse_csv( "" ) == file{} ));
    assert(( parse_csv( "a" ) == file{ { "a" } } ));
    assert(( parse_csv( "a,b\nc,d\n" ) == file{ { "a", "b" }, { "c", "d" } } ));
    assert(( parse_csv( "a,b\r\nc,d" ) == file{ { "a", "b" }, { "c", "d" } } ));
    assert(( parse_csv( ",\n" ) == file{ { "", "" } } ));
    assert(( parse_csv( "\n" ) == file{ { "" } } ));
    assert(( parse_csv( "a\n\nb" ) == file{ { "a" }, { "" }, { "b" } } ));
    assert(( parse_csv( "\"a,b\",c" ) == file{ { "a,b", "c" } } ));
    assert(( parse_csv( "\"x\"\"y\"" ) == file{ { "x\"y" } } ));
    assert(( parse_csv( "\"\"" ) == file{ { "" } } ));
    assert(( parse_csv( "\"two\nlines\",\"\"\n1" ) ==
             file{ { "two\nlines", "" }, { "1" } } ));
    assert(( parse_csv( "a b , c " ) == file{ { "a b ", " c " } } ));

    assert( !parse_csv( "\"abc" ) );
    assert( !parse_csv( "\"abc\"d" ) );
    assert( !parse_csv( "ab\"c" ) );
    assert( !parse_csv( "a\rb" ) );
    assert( !parse_csv( "\"a\"\"" ) );

    return 0;
}
