#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

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

/* Type errors found by inference are notoriously hard to explain:
 * the place where unification fails may be far from the actual
 * mistake. The least an implementation can do is to report the
 * expression at which the failure happened, and the two types that did
 * not fit. Write a variant of ‹p2_poly› which, instead of
 * ‹std::nullopt›, returns a message ‹LINE:COLUMN: TEXT›, where the
 * position is that of the first character of the expression given
 * below, and ‹TEXT› is one of:
 *
 *  • ‹unbound variable X›,
 *  • ‹cannot unify T with U› – where ‹T› is the type of the expression
 *    at the reported position, and ‹U› is the type it was required to
 *    have, both «after» applying the substitution found so far, and
 *    normalized «together» (‹T› first, then ‹U›, with one shared
 *    renaming),
 *  • ‹infinite type› – the occurs check failed,
 *  • ‹cannot compare T› – ‹T› is the (function) type of the first
 *    operand of ‹=› or ‹!=›, reported at that operand,
 *  • ‹bad form› – at a malformed list,
 *  • ‹syntax error› – at ‹1:1›, if the program cannot be read.
 *
 * Inference proceeds left to right, as in ‹d2_infer›, and the reported
 * expression is:
 *
 *  • for an operand of an operator, ‹and›, ‹or› or ‹not› – the operand,
 *    required to be ‹int› or ‹bool›,
 *  • for ‹if› – the condition (required to be ‹bool›), or the «else»
 *    branch (required to have the type of the «then» branch),
 *  • for ‹=› and ‹!=› – the second operand (required to have the type of
 *    the first one),
 *  • for a call – the «whole call»; ‹T› is the type of the callee and
 *    ‹U› is ‹(-> A₁ … Aₙ 'r)›, where ‹Aᵢ› are the types of the arguments
 *    and ‹'r› is a fresh variable for the result. */

// std::expected< std::string, std::string > check( std::string_view program );

int main()
{
    auto err = []( std::string_view p ) { return check( p ).error(); };

    assert( check( "(let id (fn (x) x) (id id))" ) == "(-> 'a 'a)" );

    assert( err( "(+ 1 #t)" ) == "1:6: cannot unify bool with int" );
    assert( err( "(fn (x)\n  (if x x 1))" ) == "2:11: cannot unify int with bool" );
    assert( err( "(fn (f) (+ (f 1) (f #t)))" ) == "1:18: cannot unify (-> int int) with (-> bool 'a)" );
    assert( err( "(fn (x) (x x))" ) == "1:9: infinite type" );
    assert( err( "(let f (fn (x y) x) (f 1))" ) == "1:21: cannot unify (-> 'a 'b 'a) with (-> int 'c)" );
    assert( err( "(fn (g) (if (g 1) (g 2) 3))" ) == "1:25: cannot unify int with bool" );
    assert( err( "(= (fn (x) x) 1)" ) == "1:4: cannot compare (-> 'a 'a)" );
    assert( err( "(fn (a b) (= a (+ b 1)) (not a))" ) == "1:1: bad form" );
    assert( err( "(fn (a) (if (= a 1) (not a) #t))" ) == "1:26: cannot unify int with bool" );
    assert( err( "(+ 1 nope)" ) == "1:6: unbound variable nope" );
    assert( err( "(1 2)" ) == "1:1: cannot unify int with (-> int 'a)" );
    assert( err( ")" ) == "1:1: syntax error" );

    return 0;
}
