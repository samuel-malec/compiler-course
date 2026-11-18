#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/* The programs in this exercise are written as S-expressions, so
 * that we can concentrate on evaluation instead of parsing. The
 * function ‹read› (provided) turns the text into a tree of ‹sexp›
 * nodes: each node is either an «atom» – a number, a name, a string
 * literal «including» its quotes (no escapes), or anything else
 * without spaces and parentheses – or a «list» of nodes. ‹offset›
 * is the position of the first character of the node in the input.
 * The function ‹write› does the opposite of ‹read›. If the input is
 * malformed, ‹read› throws ‹std::invalid_argument›. */

struct sexp
{
    std::string atom;           /* non-empty for atoms */
    std::vector< sexp > items;  /* elements of a list */
    std::size_t offset = 0;

    bool is_atom() const { return !atom.empty(); }
    bool is_list() const { return atom.empty(); }
};

void skip_space( std::string_view s, std::size_t &pos )
{
    while ( pos < s.size() && ( s[ pos ] == ' ' || s[ pos ] == '\n' ) )
        ++ pos;
}

sexp read_at( std::string_view s, std::size_t &pos )
{
    skip_space( s, pos );
    sexp out{ "", {}, pos };

    if ( pos >= s.size() || s[ pos ] == ')' )
        throw std::invalid_argument( "unexpected end of list or input" );

    if ( s[ pos ] == '(' )
    {
        for ( ++ pos; skip_space( s, pos ), pos >= s.size() || s[ pos ] != ')'; )
            out.items.push_back( read_at( s, pos ) );
        ++ pos;
        return out;
    }

    std::size_t start = pos;

    if ( s[ pos ] == '"' )
    {
        pos = s.find( '"', pos + 1 );
        if ( pos == s.npos )
            throw std::invalid_argument( "unterminated string" );
        ++ pos;
    }
    else
        while ( pos < s.size() && std::string_view( " \n()" ).find( s[ pos ] ) == s.npos )
            ++ pos;

    out.atom = s.substr( start, pos - start );
    return out;
}

sexp read( std::string_view s )
{
    std::size_t pos = 0;
    sexp e = read_at( s, pos );
    skip_space( s, pos );
    if ( pos != s.size() )
        throw std::invalid_argument( "trailing input" );
    return e;
}

std::string write( const sexp &e )
{
    if ( e.is_atom() )
        return e.atom;

    std::string out;
    for ( const auto &i : e.items )
        out += ( out.empty() ? "" : " " ) + write( i );
    return "(" + out + ")";
}

/* A «profiler» tells the programmer which parts of a program take the
 * most time. The simplest kind counts how many times each statement
 * executes. Write a function ‹profile› which runs a program in the
 * full core language (from the introduction) and counts the
 * executions of each statement.
 *
 * Statements are numbered in «pre-order»: the whole program (which is
 * a single statement) is number 0, and the statements inside a
 * statement are numbered right after it, in order – e.g. in ‹(block
 * (var i 0) (while c (set i 1)))›, the ‹block› is 0, the ‹var› is 1,
 * the ‹while› is 2 and the ‹set› is 3. Each statement in the program
 * gets a number, whether it executes or not. The result has one entry
 * per statement, in order of the numbers. A statement which starts
 * executing counts once, even if it is a loop.
 *
 * Return ‹std::nullopt› for the errors of ‹e1_print›. */

// std::optional< std::vector< long > > profile( std::string_view program );

int main()
{
    using v = std::vector< long >;

    assert(( profile( "(print 1)" ) == v{ 1 } ));
    assert(( profile( "(block (var i 0) (while (< i 3) (set i (+ i 1))))" ) == v{ 1, 1, 1, 3 } ));
    assert(( profile( "(block (var x 1) (if x (print 1) (print 2)))" ) == v{ 1, 1, 1, 1, 0 } ));
    assert(( profile( "(block (var i 0) (while (< i 10) (block (set i (+ i 1))"
                      " (if (= (% i 3) 0) (print i)))))" ) == v{ 1, 1, 1, 10, 10, 10, 3 } ));
    assert(( profile( "(block (var i 0) (while (< i 2) (block (set i (+ i 1)) (var j 0)"
                      " (while (< j i) (set j (+ j 1))))))" ) == v{ 1, 1, 1, 2, 2, 2, 2, 3 } ));
    assert(( profile( "(if 0 (block (print 1) (print 2)))" ) == v{ 1, 0, 0, 0 } ));

    assert( !profile( "(print x)" ) );

    return 0;
}
