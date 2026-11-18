#include <cassert>
#include <optional>
#include <string_view>

/* Write a function ‹tautology› which parses a formula of
 * propositional logic and decides whether it is a «tautology» (true
 * under every assignment of truth values to its variables). The
 * syntax, from the loosest to the tightest binding:
 *
 *  • ‹a <-> b› – equivalence, left-associative,
 *  • ‹a -> b›  – implication, «right»-associative (‹a -> b -> c› is
 *    ‹a -> (b -> c)›),
 *  • ‹a | b›   – disjunction, left-associative,
 *  • ‹a & b›   – conjunction, left-associative,
 *  • ‹!a›      – negation (prefix),
 *  • atoms: variables (non-empty sequences of lowercase letters), the
 *    constants ‹1› (true) and ‹0› (false), and parenthesised formulas.
 *
 * Spaces may appear between tokens. If the input is not a valid
 * formula, return ‹std::nullopt›.
 *
 * Consider first parsing the formula into a tree (a vector of nodes
 * which refer to each other by index is a simple representation),
 * and then evaluating the tree for each assignment. You can assume
 * there are at most 16 distinct variables. */

// std::optional< bool > tautology( std::string_view formula );

int main()
{
    assert( tautology( "1" ) == true );
    assert( tautology( "0" ) == false );
    assert( tautology( "p" ) == false );
    assert( tautology( "p | !p" ) == true );
    assert( tautology( "p & !p" ) == false );
    assert( tautology( "p -> p" ) == true );
    assert( tautology( "p -> q" ) == false );
    assert( tautology( "(p -> q) -> (!q -> !p)" ) == true );
    assert( tautology( "p -> q -> p" ) == true );        /* right assoc */
    assert( tautology( "(p -> q) -> p" ) == false );
    assert( tautology( "((p -> q) -> p) -> p" ) == true ); /* Peirce */
    assert( tautology( "p & q -> p" ) == true );
    assert( tautology( "p | q & r <-> (p | q) & (p | r)" ) == true );
    assert( tautology( "!(a & b) <-> !a | !b" ) == true );
    assert( tautology( "a <-> b" ) == false );
    assert( tautology( "p & !p -> anything" ) == true );
    assert( tautology( "!!x <-> x" ) == true );
    assert( tautology( "x <-> y <-> (x <-> y)" ) == true );

    assert( !tautology( "" ) );
    assert( !tautology( "p &" ) );
    assert( !tautology( "(p" ) );
    assert( !tautology( "p q" ) );
    assert( !tautology( "p => q" ) );
    assert( !tautology( "p - > q" ) );
    assert( !tautology( "P" ) );

    return 0;
}
