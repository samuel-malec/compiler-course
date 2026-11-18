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

/* Many language constructs are just convenient abbreviations for
 * combinations of other constructs: they are «derived forms», or
 * ‘syntactic sugar’. An implementation can save a lot of work by
 * translating them away («desugaring») right after parsing, so that
 * the rest of the implementation only needs to deal with a small
 * «core» language.
 *
 * Write a function ‹desugar› which translates the following derived
 * forms (‹…› means ‘zero or more of the previous item’):
 *
 *  • ‹(cond (c₁ e₁) (c₂ e₂) … (else e))› ⇒ ‹(if c₁ e₁ (if c₂ e₂ … e))›;
 *    the ‹else› clause is optional, and if it is missing, the final
 *    ‹e› is ‹#f›; an ‹else› clause anywhere but last is an error,
 *  • ‹(let* ((x₁ v₁) (x₂ v₂) …) b)› ⇒ ‹(let x₁ v₁ (let x₂ v₂ … b))›;
 *    with no bindings at all, the result is just ‹b›,
 *  • ‹(and)› ⇒ ‹#t›, ‹(and a)› ⇒ ‹a›, ‹(and a b c …)› ⇒
 *    ‹(and a (and b (and c …)))›, i.e. a right-nested chain of binary
 *    ‹and›s, and the same for ‹or› (with ‹#f› for the empty case),
 *  • ‹(when c e)› ⇒ ‹(if c e #f)›, ‹(unless c e)› ⇒ ‹(if c #f e)›.
 *
 * All other lists are left as they are, except that their elements
 * are desugared (and so are all the sub-expressions of the forms
 * above – including the results of the translation, e.g. a ‹cond›
 * which appears inside a ‹let*› binding). Atoms are unchanged.
 *
 * The result should be written using ‹write›. Return ‹std::nullopt›
 * if the input cannot be read, or if one of the derived forms is
 * malformed: a ‹cond› clause which is not a list of exactly 2
 * elements, a ‹let*› whose bindings are not a list of 2-element lists
 * with an atom first, or ‹let*›, ‹when› or ‹unless› with a wrong
 * number of elements. */

// std::optional< std::string > desugar( std::string_view program );

int main()
{
    assert( desugar( "x" ) == "x" );
    assert( desugar( "(+ 1 2)" ) == "(+ 1 2)" );
    assert( desugar( "(cond (a 1) (b 2) (else 3))" ) == "(if a 1 (if b 2 3))" );
    assert( desugar( "(cond (a 1))" ) == "(if a 1 #f)" );
    assert( desugar( "(cond (else 7))" ) == "7" );
    assert( desugar( "(cond)" ) == "#f" );
    assert( desugar( "(let* ((x 1) (y x)) (+ x y))" ) == "(let x 1 (let y x (+ x y)))" );
    assert( desugar( "(let* () 5)" ) == "5" );
    assert( desugar( "(and)" ) == "#t" );
    assert( desugar( "(or)" ) == "#f" );
    assert( desugar( "(and a)" ) == "a" );
    assert( desugar( "(and a b)" ) == "(and a b)" );
    assert( desugar( "(and a b c d)" ) == "(and a (and b (and c d)))" );
    assert( desugar( "(or a b c)" ) == "(or a (or b c))" );
    assert( desugar( "(when c (f x))" ) == "(if c (f x) #f)" );
    assert( desugar( "(unless c 1)" ) == "(if c #f 1)" );
    assert( desugar( "(f (when a b) (and))" ) == "(f (if a b #f) #t)" );
    assert( desugar( "(let* ((x (cond (a 1) (else 2)))) (and x y z))" ) ==
            "(let x (if a 1 2) (and x (and y z)))" );
    assert( desugar( "(cond ((and a b c) (let* ((x 1)) x)))" ) ==
            "(if (and a (and b c)) (let x 1 x) #f)" );
    assert( desugar( "()" ) == "()" );

    assert( !desugar( "(cond (a 1) (else 2) (b 3))" ) );
    assert( !desugar( "(cond (a 1 2))" ) );
    assert( !desugar( "(cond a)" ) );
    assert( !desugar( "(let* (x 1) x)" ) );
    assert( !desugar( "(let* (((x) 2)) x)" ) );
    assert( !desugar( "(let* ((x 1)))" ) );
    assert( !desugar( "(when c)" ) );
    assert( !desugar( "(unless a b c)" ) );
    assert( !desugar( "(f (when))" ) );
    assert( !desugar( "(+ 1" ) );

    return 0;
}
