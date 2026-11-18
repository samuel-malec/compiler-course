#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
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

/* In this exercise, we will evaluate programs using «small-step»
 * semantics: instead of computing the value of a program in one go,
 * we will rewrite the program one small step at a time, until it
 * becomes a value. The language:
 *
 *  • integer literals (digits with an optional leading ‹-›) are the
 *    «values»; any other atom is a variable,
 *  • ‹(+ a b)›, ‹(- a b)›, ‹(* a b)›,
 *  • ‹(let x v b)›.
 *
 * A single «step» rewrites the program at exactly one place, as
 * follows:
 *
 *  • for ‹(op a b)›: if ‹a› is not a value, take a step in ‹a›;
 *    otherwise, if ‹b› is not a value, take a step in ‹b›; otherwise,
 *    replace the whole expression by the result (a number),
 *  • for ‹(let x v b)›: if ‹v› is not a value, take a step in ‹v›;
 *    otherwise, replace the whole ‹let› by ‹b› with the value ‹v›
 *    «substituted» for the free occurrences of ‹x› (a nested
 *    ‹(let x …)› shadows ‹x› in its body, but not in its value).
 *
 * Since only numbers are ever substituted, there is no danger of
 * variable capture (cf. ‹03/p4_subst›). If a program is not a value,
 * but no step is possible (because of a free variable), the program
 * is «stuck».
 *
 * Write a function ‹steps›, which returns the whole sequence of
 * programs (written with ‹write›), starting with the input and ending
 * with a value. Return ‹std::nullopt› if the program gets stuck or if
 * it is malformed. */

// std::optional< std::vector< std::string > > steps( std::string_view program );

struct stuck {};

bool is_value( const sexp &e )
{
    const std::string &a = e.atom;
    std::size_t start = !a.empty() && a[ 0 ] == '-' ? 1 : 0;
    return a.size() > start && a.find_first_not_of( "0123456789", start ) == a.npos;
}

sexp number( long n ) { return sexp{ std::to_string( n ), {}, 0 }; }

/* Substitute the number ‹v› for free occurrences of ‹x› in ‹e›. */

sexp subst( const sexp &e, const std::string &x, const sexp &v )
{
    if ( e.is_atom() )
        return e.atom == x ? v : e;

    sexp out = e;
    bool is_let = e.items.size() == 4 && e.items[ 0 ].atom == "let";
    bool shadows = is_let && e.items[ 1 ].atom == x;

    for ( std::size_t i = 1; i < out.items.size(); ++ i )
        if ( !( is_let && i == 1 ) &&      /* the bound name itself */
             !( shadows && i == 3 ) )      /* the body of a shadowing let */
            out.items[ i ] = subst( e.items[ i ], x, v );

    return out;
}

/* Perform a single step; throws ‹stuck› if no step is possible. */

sexp step( const sexp &e )
{
    if ( e.is_atom() )
        throw stuck(); /* a variable (values never get here) */

    if ( e.items.size() != 4 && e.items.size() != 3 )
        throw stuck();

    const std::string &op = e.items[ 0 ].atom;
    sexp out = e;

    if ( op == "let" && e.items.size() == 4 && e.items[ 1 ].is_atom() )
    {
        if ( !is_value( e.items[ 2 ] ) )
        {
            out.items[ 2 ] = step( e.items[ 2 ] );
            return out;
        }
        return subst( e.items[ 3 ], e.items[ 1 ].atom, e.items[ 2 ] );
    }

    if ( e.items.size() != 3 || ( op != "+" && op != "-" && op != "*" ) )
        throw stuck();

    for ( int i : { 1, 2 } )
        if ( !is_value( e.items[ i ] ) )
        {
            out.items[ i ] = step( e.items[ i ] );
            return out;
        }

    long a = std::stol( e.items[ 1 ].atom ), b = std::stol( e.items[ 2 ].atom );
    return number( op == "+" ? a + b : op == "-" ? a - b : a * b );
}

std::optional< std::vector< std::string > > steps( std::string_view program )
{
    try
    {
        sexp e = read( program );
        std::vector< std::string > out{ write( e ) };

        while ( !is_value( e ) )
        {
            e = step( e );
            out.push_back( write( e ) );
        }

        return out;
    }
    catch ( const stuck & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    using v = std::vector< std::string >;

    assert(( steps( "7" ) == v{ "7" } ));
    assert(( steps( "(+ 1 2)" ) == v{ "(+ 1 2)", "3" } ));
    assert(( steps( "(- 1 5)" ) == v{ "(- 1 5)", "-4" } ));
    assert(( steps( "(*  -4   2)" ) == v{ "(* -4 2)", "-8" } ));

    assert(( steps( "(+ (* 2 3) (- 5 1))" ) ==
             v{ "(+ (* 2 3) (- 5 1))", "(+ 6 (- 5 1))", "(+ 6 4)", "10" } ));

    assert(( steps( "(let x (+ 1 2) (* x x))" ) ==
             v{ "(let x (+ 1 2) (* x x))", "(let x 3 (* x x))", "(* 3 3)", "9" } ));

    assert(( steps( "(let x 1 (let x (+ x 1) x))" ) ==
             v{ "(let x 1 (let x (+ x 1) x))", "(let x (+ 1 1) x)",
                "(let x 2 x)", "2" } ));

    assert(( steps( "(let a 2 (let b (* a a) (+ a b)))" ) ==
             v{ "(let a 2 (let b (* a a) (+ a b)))", "(let b (* 2 2) (+ 2 b))",
                "(let b 4 (+ 2 b))", "(+ 2 4)", "6" } ));

    assert( !steps( "x" ) );
    assert( !steps( "(+ x 1)" ) );
    assert( !steps( "(let y 2 (+ y z))" ) );
    assert( !steps( "(+ 1 2 3)" ) );
    assert( !steps( "(% 1 2)" ) );

    return 0;
}
