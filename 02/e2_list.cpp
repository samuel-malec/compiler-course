#include <cassert>
#include <optional>
#include <string_view>
#include <vector>

/* Write a recursive descent parser (it will be a very short one) for
 * lists of integers, given by the following grammar:
 *
 *     list  = '[', [ items ], ']' ;
 *     items = int, { ',', int } ;
 *     int   = [ '-' ], digit, { digit } ;
 *
 * Any number of spaces may appear before or after any of the tokens
 * ‹[›, ‹]›, ‹,› and ‹int› (but not inside an ‹int›, i.e. not between
 * the minus sign and the digits). The entire input must be a single
 * ‹list›. The function ‹parse_list› should return the integers, or
 * ‹std::nullopt› if the input does not match the grammar. You can
 * assume that the numbers fit into an ‹int›. */

// std::optional< std::vector< int > > parse_list( std::string_view s );

int main()
{
    using v = std::vector< int >;

    assert(( parse_list( "[]" ) == v{} ));
    assert(( parse_list( "[1]" ) == v{ 1 } ));
    assert(( parse_list( "[1,2,3]" ) == v{ 1, 2, 3 } ));
    assert(( parse_list( "  [ -1 , 20,3 ]  " ) == v{ -1, 20, 3 } ));
    assert(( parse_list( "[ ]" ) == v{} ));

    assert( !parse_list( "" ) );
    assert( !parse_list( "[" ) );
    assert( !parse_list( "[1,]" ) );
    assert( !parse_list( "[,1]" ) );
    assert( !parse_list( "[1 2]" ) );
    assert( !parse_list( "[- 1]" ) );
    assert( !parse_list( "[1] x" ) );
    assert( !parse_list( "1, 2" ) );
    assert( !parse_list( "[[1]]" ) );

    return 0;
}
