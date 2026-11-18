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

/* Extend the evaluator from ‹e1_apply› with recursive definitions:
 *
 *     (letrec ((f₁ e₁) (f₂ e₂) …) body)
 *
 * Each ‹eᵢ› must be a function expression ‹(fn …)› (check this before
 * evaluating anything). All the names ‹fᵢ› are visible in all the
 * ‹eᵢ› and in ‹body›, so the functions can call themselves and each
 * other. As before, ‹eval› returns the integer value of the program,
 * or ‹std::nullopt› on any error (see ‹e1_apply›), including a
 * malformed ‹letrec› or a duplicate name in one ‹letrec›. See
 * ‹d1_closures› for the idea, and the introduction for a note on
 * memory management. */

// std::optional< long > eval( std::string_view program );

int main()
{
    assert( eval( "(letrec ((fact (fn (n) (if (= n 0) 1 (* n (fact (- n 1))))))) (fact 10))" ) == 3628800 );
    assert( eval( "(letrec ((fib (fn (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2))))))) (fib 15))" ) == 610 );
    assert( eval( "(letrec ((even (fn (n) (if (= n 0) 1 (odd (- n 1)))))"
                  "         (odd (fn (n) (if (= n 0) 0 (even (- n 1))))))"
                  " (+ (* 10 (even 10)) (odd 7)))" ) == 11 );
    assert( eval( "(letrec ((ack (fn (m n) (if (= m 0) (+ n 1)"
                  "   (if (= n 0) (ack (- m 1) 1) (ack (- m 1) (ack m (- n 1))))))))"
                  " (ack 2 3))" ) == 9 );

    /* a recursive function which returns a closure */
    assert( eval( "(letrec ((adder (fn (n) (if (= n 0) (fn (x) x)"
                  "   (let r (adder (- n 1)) (fn (x) (+ 1 (r x))))))))"
                  " ((adder 5) 10))" ) == 15 );

    /* a letrec function used outside of the letrec */
    assert( eval( "(let sq (letrec ((sq (fn (n) (* n n)))) sq) (sq 9))" ) == 81 );

    /* the inner letrec shadows the outer f */
    assert( eval( "(letrec ((f (fn (x) 1))) (+ (f 0) (letrec ((f (fn (x) (if x (f 0) 10)))) (f 1))))" ) == 11 );

    assert( !eval( "(letrec ((x 1)) x)" ) );
    assert( !eval( "(letrec ((f (fn () 1)) (f (fn () 2))) (f))" ) );
    assert( !eval( "(letrec (f (fn () 1)) (f))" ) );
    assert( !eval( "(letrec ((f (fn (n) (g n)))) (f 1))" ) );

    return 0;
}
