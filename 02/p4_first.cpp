#include <cassert>
#include <map>
#include <set>
#include <string>
#include <vector>

/* In this exercise, we will treat grammars as data. A grammar maps
 * each nonterminal to a list of alternatives, and each alternative
 * is a sequence of symbols. Any symbol which is a key in the map is a
 * nonterminal, any other symbol is a terminal. An empty sequence is
 * an ‹ε› alternative. For example, this grammar:
 *
 *     E  → T E'
 *     E' → + T E' | ε
 *
 * is represented as ‹{ { "E", { { "T", "E'" } } },
 *                      { "E'", { { "+", "T", "E'" }, {} } } }›
 * (with ‹T› being a nonterminal defined elsewhere in the map). */

using symbol = std::string;
using sequence = std::vector< symbol >;
using grammar = std::map< symbol, std::vector< sequence > >;

/* The ‹FIRST› set of a nonterminal ⟦A⟧ is the set of terminals which
 * can appear «first» in some string derived from ⟦A⟧. If ⟦A⟧ can
 * derive the empty string (⟦A⟧ is «nullable»), we also put the empty
 * string ‹""› into its ‹FIRST› set. For a recursive descent parser,
 * the ‹FIRST› sets of the alternatives tell us which alternative to
 * pick, based on the next token.
 *
 * Write a function ‹first› which computes the ‹FIRST› set of every
 * nonterminal in the grammar. The definitions are recursive, so the
 * usual approach is a «fixpoint» computation: start with all sets
 * empty, then repeatedly go over all rules, and for each rule
 * ⟦A → X₁ X₂ … Xₙ⟧:
 *
 *  • add ⟦FIRST(X₁)⟧ (without ‹""›) to ⟦FIRST(A)⟧, where the ‹FIRST›
 *    set of a terminal ⟦t⟧ is just ⟦{ t }⟧,
 *  • if ⟦X₁⟧ is nullable (its set contains ‹""›), continue with
 *    ⟦X₂⟧ in the same way, and so on,
 *  • if all of ⟦X₁ … Xₙ⟧ are nullable (in particular if ⟦n = 0⟧),
 *    add ‹""› to ⟦FIRST(A)⟧,
 *
 * until nothing changes anymore. */

// std::map< symbol, std::set< symbol > > first( const grammar &g );

int main()
{
    using fs = std::map< symbol, std::set< symbol > >;

    grammar expr =
    {
        { "E",  { { "T", "E'" } } },
        { "E'", { { "+", "T", "E'" }, {} } },
        { "T",  { { "F", "T'" } } },
        { "T'", { { "*", "F", "T'" }, {} } },
        { "F",  { { "(", "E", ")" }, { "id" } } },
    };

    assert(( first( expr ) == fs{ { "E",  { "(", "id" } },
                                  { "E'", { "+", "" } },
                                  { "T",  { "(", "id" } },
                                  { "T'", { "*", "" } },
                                  { "F",  { "(", "id" } } } ));

    grammar nullable =
    {
        { "S", { { "A", "B", "c" } } },
        { "A", { { "a" }, {} } },
        { "B", { { "b" }, {} } },
    };

    assert(( first( nullable ) == fs{ { "S", { "a", "b", "c" } },
                                      { "A", { "a", "" } },
                                      { "B", { "b", "" } } } ));

    grammar empty =
    {
        { "S", { { "A", "B" } } },
        { "A", { {} } },
        { "B", { {} } },
    };

    assert(( first( empty ) == fs{ { "S", { "" } }, { "A", { "" } },
                                   { "B", { "" } } } ));

    grammar leftrec =
    {
        { "E", { { "E", "+", "T" }, { "T" } } },
        { "T", { { "id" } } },
    };

    assert(( first( leftrec ) == fs{ { "E", { "id" } }, { "T", { "id" } } } ));

    grammar mutual =
    {
        { "A", { { "B", "x" }, { "y" } } },
        { "B", { { "A", "z" }, {} } },
    };

    assert(( first( mutual ) == fs{ { "A", { "x", "y" } },
                                    { "B", { "x", "y", "" } } } ));

    grammar useless = { { "S", { { "S", "a" } } } };
    assert(( first( useless ) == fs{ { "S", {} } } ));

    return 0;
}
