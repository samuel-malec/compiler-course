#include <cassert>
#include <string>
#include <vector>

/* Write a function ‹blocks› which splits a TAC program (see the
 * introduction; given as lines of text) into «basic blocks». A new
 * block starts at the first line, at every label line (‹NAME:›), and at
 * the line after every ‹goto› and ‹if … goto›. Each block is returned as
 * the list of its lines, including its label line (if any). An empty
 * program has no blocks. You only need to recognize labels (lines
 * ending with ‹:›) and jumps (lines starting with ‹goto › or ‹if ›). */

// std::vector< std::vector< std::string > > blocks( const std::vector< std::string > &program );

int main()
{
    using b = std::vector< std::vector< std::string > >;

    assert(( blocks( {} ) == b{} ));
    assert(( blocks( { "x = 1", "print x" } ) == b{ { "x = 1", "print x" } } ));
    assert(( blocks( { "x = 1", "L:", "print x" } ) == b{ { "x = 1" }, { "L:", "print x" } } ));
    assert(( blocks( { "if x goto L", "y = 1", "L:", "L2:", "goto L" } ) ==
             b{ { "if x goto L" }, { "y = 1" }, { "L:" }, { "L2:", "goto L" } } ));
    assert(( blocks( { "goto A", "goto B", "A:", "B:" } ) ==
             b{ { "goto A" }, { "goto B" }, { "A:" }, { "B:" } } ));

    return 0;
}
