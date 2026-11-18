#include <algorithm>
#include <cassert>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

/* A lexer (or any other phase) which reports an error must tell the
 * user «where» the error is. In this demo, we will look at how to
 * turn an «offset» into the input (which is what lexers naturally
 * keep track of) into a «line» and «column» (which is what humans
 * want to see), and how to format a nice error message, like this:
 *
 *     input:2:5: error: unexpected character '$'
 *         let $x = 1
 *             ^
 *
 * There are two ways to get line numbers. One is to count newlines
 * while lexing; the other is to remember just the offset in each
 * token and compute the line and column only when an error message
 * is actually needed. The second is usually better: errors are rare,
 * tokens are many, and an offset is smaller than a line-column pair.
 * We will take the second approach here.
 *
 * Lines and columns are traditionally numbered from 1. We will count
 * columns in «bytes», which is simple, but not quite right for
 * non-ASCII text (and tabs) – real compilers go to considerable
 * lengths to get this right. */

struct location
{
    int line, column;
    bool operator==( const location & ) const = default;
};

/* Computing the location of a single offset is simple: count the
 * newlines before it. The column is the distance from the last
 * newline (or from the start of input). */

location locate( std::string_view src, std::size_t offset )
{
    location loc{ 1, 1 };

    for ( std::size_t i = 0; i < offset && i < src.size(); ++ i )
        if ( src[ i ] == '\n' )
            loc.line ++, loc.column = 1;
        else
            loc.column ++;

    return loc;
}

/* If we need to locate many offsets, scanning from the beginning
 * every time is wasteful (quadratic in the worst case). Instead, we
 * can build an index of line starts once, then find the line of any
 * offset by binary search. This is what most compilers do. */

struct line_index
{
    std::vector< std::size_t > starts;

    explicit line_index( std::string_view src ) : starts{ 0 }
    {
        for ( std::size_t i = 0; i < src.size(); ++ i )
            if ( src[ i ] == '\n' )
                starts.push_back( i + 1 );
    }

    location locate( std::size_t offset ) const
    {
        /* Find the last line start which is ‹<= offset›. The
         * ‹upper_bound› gives us the first one which is ‹> offset›,
         * so we step one back. */
        auto it = std::upper_bound( starts.begin(), starts.end(), offset );
        int line = it - starts.begin();
        return { line, int( offset - *std::prev( it ) ) + 1 };
    }
};

/* To print the offending line, we need its text: everything from
 * the start of the line up to (not including) the next newline. */

std::string_view line_text( std::string_view src, std::size_t offset )
{
    auto begin = src.rfind( '\n', offset == 0 ? 0 : offset - 1 );
    begin = ( begin == src.npos || offset == 0 ) ? 0 : begin + 1;
    auto end = src.find( '\n', begin );
    return src.substr( begin, end == src.npos ? src.npos : end - begin );
}

/* And finally, the error message itself. The caret (‹^›) is placed
 * under the offending column. */

std::string diagnose( std::string_view file, std::string_view src,
                      std::size_t offset, std::string_view message )
{
    auto [ line, col ] = locate( src, offset );
    std::string out;

    out += std::string( file ) + ":" + std::to_string( line ) + ":" +
           std::to_string( col ) + ": error: " + std::string( message );
    out += "\n    " + std::string( line_text( src, offset ) );
    out += "\n    " + std::string( col - 1, ' ' ) + "^\n";

    return out;
}

int main() /* demo */
{
    std::string_view src = "let a = 1 in\nlet $x = 1\nin a";
    std::size_t dollar = src.find( '$' );

    assert(( locate( src, 0 ) == location{ 1, 1 } ));
    assert(( locate( src, 4 ) == location{ 1, 5 } ));
    assert(( locate( src, dollar ) == location{ 2, 5 } ));
    assert(( locate( src, src.size() ) == location{ 3, 5 } ));

    line_index idx( src );
    for ( std::size_t i = 0; i <= src.size(); ++ i )
        assert( idx.locate( i ) == locate( src, i ) );

    assert( line_text( src, dollar ) == "let $x = 1" );
    assert( line_text( src, 0 ) == "let a = 1 in" );
    assert( line_text( src, src.size() - 1 ) == "in a" );

    assert( diagnose( "input", src, dollar, "unexpected character '$'" ) ==
            "input:2:5: error: unexpected character '$'\n"
            "    let $x = 1\n"
            "        ^\n" );

    return 0;
}
