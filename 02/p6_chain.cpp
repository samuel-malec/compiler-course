#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>

/* In Python, ‹a < b < c› is not an error (as in ‹tiny›) and it does
 * not mean ‹(a < b) < c› (as in C): it means ‹a < b and b < c›, with
 * ‹b› evaluated only once. In this exercise, you will write an
 * evaluator for expressions with Python-style «chained comparisons»
 * and «short-circuiting» boolean operators. The grammar is:
 *
 *     expr  = conj, { 'or', conj } ;
 *     conj  = neg, { 'and', neg } ;
 *     neg   = 'not', neg | cmp ;
 *     cmp   = sum, { cmpop, sum } ;
 *     cmpop = '==' | '!=' | '<' | '<=' | '>' | '>=' ;
 *     sum   = prod, { ( '+' | '-' ), prod } ;
 *     prod  = atom, { ( '*' | '/' ), atom } ;
 *     atom  = number | ident | '(', expr, ')' ;
 *
 * where ‹number› is a sequence of digits and ‹ident› is a sequence of
 * lowercase letters other than ‹and›, ‹or› and ‹not›. Tokens may be
 * separated by spaces (and must be, where they would otherwise run
 * together, as in ‹a and b›).
 *
 * All values are of type ‹long›; the result of a comparison or of a
 * boolean operator is 1 (true) or 0 (false), and any non-zero value
 * counts as true. The semantics:
 *
 *  • ‹x₀ op₁ x₁ op₂ x₂ … opₙ xₙ› (with comparison operators ‹opᵢ›) is
 *    true iff all of ‹xᵢ₋₁ opᵢ xᵢ› are true; the operands are
 *    evaluated left to right, each at most once, and evaluation stops
 *    as soon as one of the comparisons is false,
 *  • ‹a and b› is 0 if ‹a› is false (and then ‹b› is «not
 *    evaluated»), otherwise it is 1 if ‹b› is true and 0 otherwise,
 *  • ‹a or b› is 1 if ‹a› is true (and then ‹b› is «not evaluated»),
 *    otherwise it is 1 if ‹b› is true and 0 otherwise,
 *  • ‹not a› is 1 if ‹a› is false and 0 otherwise,
 *  • the arithmetic operators work as in C++.
 *
 * Variables are looked up in ‹env›. Return ‹std::nullopt› if the
 * input is not syntactically valid (even in a part which is not
 * evaluated), or if the evaluation divides by zero or uses a variable
 * which is not in ‹env› («only» if that part is evaluated).
 *
 * The tricky part is to «parse» a sub-expression without
 * «evaluating» it. One simple way is to give the parser a flag that
 * says whether it should evaluate. */

// std::optional< long > eval( std::string_view expr,
//                             const std::map< std::string, long > &env );

int main()
{
    std::map< std::string, long > env{ { "x", 1 }, { "y", 2 }, { "z", 3 } };
    auto ev = [ & ]( std::string_view e ) { return eval( e, env ); };

    assert( ev( "2 * 3 + 4" ) == 10 );
    assert( ev( "x + y * z" ) == 7 );
    assert( ev( "1 < 2" ) == 1 );
    assert( ev( "1 < 2 < 3" ) == 1 );
    assert( ev( "1 < 3 < 2" ) == 0 );
    assert( ev( "z > y > x" ) == 1 );
    assert( ev( "1 < 2 == 1" ) == 0 ); /* not ‹(1 < 2) == 1› */
    assert( ev( "x == 1 == x" ) == 1 );
    assert( ev( "(1 < 2) == 1" ) == 1 );
    assert( ev( "x + 1 == y" ) == 1 );
    assert( ev( "not 1 == 2" ) == 1 );
    assert( ev( "not not 5" ) == 1 );
    assert( ev( "5 and 7" ) == 1 );
    assert( ev( "0 or 0" ) == 0 );
    assert( ev( "1 == 1 and 2 == 3 or 4 > 3" ) == 1 );
    assert( ev( "x < y and y < z" ) == 1 );

    /* short-circuiting */
    assert( ev( "0 and 1 / 0" ) == 0 );
    assert( ev( "1 or 1 / 0" ) == 1 );
    assert( ev( "0 and w" ) == 0 );
    assert( ev( "2 > 3 < 1 / 0" ) == 0 );
    assert( ev( "0 and (1 or (2 / 0 == w))" ) == 0 );

    assert( !ev( "1 / 0 or 1" ) );
    assert( !ev( "1 < 2 < 1 / 0" ) );
    assert( !ev( "1 and w" ) );
    assert( !ev( "0 and (1 +" ) );
    assert( !ev( "0 and and" ) );
    assert( !ev( "1 2" ) );
    assert( !ev( "" ) );
    assert( !ev( "x <" ) );

    return 0;
}
