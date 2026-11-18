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

/* Write a function ‹tail_calls› which finds all calls in «tail
 * position» in a program in the language of ‹p1_letrec›. Positions are
 * defined as follows:
 *
 *  • the body of a ‹fn› is in tail position,
 *  • if an ‹if› is in tail position, so are its two branches (but not
 *    the condition),
 *  • if a ‹let› or ‹letrec› is in tail position, so is its body (but
 *    not the bound expressions),
 *  • nothing else is in tail position: operands of operators, the
 *    function and the arguments of a call, and in particular the whole
 *    program (which is not inside any function).
 *
 * The result lists the tail calls (written with ‹write›) in
 * «pre-order» (in the order in which their opening parenthesis
 * appears in the program). You can assume that the input is well
 * formed. */

// std::vector< std::string > tail_calls( std::string_view program );

int main()
{
    using v = std::vector< std::string >;

    assert(( tail_calls( "(f 1)" ) == v{} ));
    assert(( tail_calls( "(fn (x) (f x))" ) == v{ "(f x)" } ));
    assert(( tail_calls( "(fn (x) (+ 1 (f x)))" ) == v{} ));
    assert(( tail_calls( "(fn (n) (if (p n) (g n) (h (k n))))" ) == v{ "(g n)", "(h (k n))" } ));
    assert(( tail_calls( "(fn (n) (let a (f n) (g a)))" ) == v{ "(g a)" } ));
    assert(( tail_calls( "(letrec ((loop (fn (n acc) (if (= n 0) acc (loop (- n 1) (* acc n))))))"
                         " (loop 5 1))" ) == v{ "(loop (- n 1) (* acc n))" } ));
    assert(( tail_calls( "(fn (f) ((f 1) (fn (y) (f y))))" ) == v{ "((f 1) (fn (y) (f y)))", "(f y)" } ));
    assert(( tail_calls( "(fn () (letrec ((g (fn () (h)))) (g)))" ) == v{ "(h)", "(g)" } ));

    return 0;
}
