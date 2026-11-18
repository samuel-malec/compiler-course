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

/* Extend the evaluator from ‹e1_apply› with «mutable variables» and
 * output:
 *
 *  • ‹(set x e)› – assign the value of ‹e› to the nearest visible
 *    variable ‹x› (an error if there is none); the value of the
 *    expression is the assigned value,
 *  • ‹(do e₁ e₂ … eₙ)› – evaluate the expressions in order; the value
 *    is that of ‹eₙ› (‹n ≥ 1›),
 *  • ‹(print e)› – append the value of ‹e› (which must be an integer)
 *    to the output; the value of the expression is the same value.
 *
 * Closures capture «variables», not values: if two closures (or a
 * closure and the code around it) refer to the same variable, they
 * all see the effect of a ‹set›. On the other hand, a call binds the
 * parameters to «copies» of the argument values (call-by-value), so
 * assigning to a parameter has no effect on the caller.
 *
 * The function ‹run› returns the output of the program, or
 * ‹std::nullopt› on errors (see ‹e1_apply›; the value of the whole
 * program does not matter, it can also be a function). */

// std::optional< std::vector< long > > run( std::string_view program );

int main()
{
    using v = std::vector< long >;

    auto counter = "(let make (fn () (let n 0 (fn () (set n (+ n 1)))))"
                   " (let a (make) (let b (make)"
                   "  (do (print (a)) (print (a)) (print (b)) (print (a))))))";
    assert(( run( counter ) == v{ 1, 2, 1, 3 } ));

    /* an «object» with two «methods» sharing one variable */
    auto account = "(let acct (let balance 100"
                   "   (fn (op amount) (if (= op 0) balance (set balance (- balance amount)))))"
                   " (do (acct 1 30) (acct 1 20) (print (acct 0 0))))";
    assert(( run( account ) == v{ 50 } ));

    /* the closure sees later changes of the captured variable */
    assert(( run( "(let x 1 (let f (fn () x) (do (set x 2) (print (f)))))" ) == v{ 2 } ));

    /* call-by-value */
    assert(( run( "(let f (fn (x) (set x 5)) (let y 1 (do (f y) (print y))))" ) == v{ 1 } ));

    /* set inside a nested let affects the outer variable */
    assert(( run( "(let x 0 (do (let y 1 (set x (+ x y))) (print x)))" ) == v{ 1 } ));
    assert(( run( "(do (print 1) (print (+ (print 2) (print 3))))" ) == v{ 1, 2, 3, 5 } ));
    assert(( run( "(fn (x) x)" ) == v{} ));

    assert( !run( "(set x 1)" ) );
    assert( !run( "(do)" ) );
    assert( !run( "(print (fn () 1))" ) );

    return 0;
}
