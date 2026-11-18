#include <cassert>
#include <map>
#include <string>
#include <vector>

/* As explained in the introduction, recursive descent cannot handle
 * left-recursive rules. In this exercise, we will transform grammars
 * to get rid of «immediate» left recursion (where a nonterminal
 * appears first in one of its own alternatives). The representation
 * of grammars is the same as in ‹p4_first›: */

using symbol = std::string;
using sequence = std::vector< symbol >;
using grammar = std::map< symbol, std::vector< sequence > >;

/* The transformation is as follows. First, remove all alternatives of
 * the form ⟦A → A⟧ (they are useless). Then, for each nonterminal
 * ⟦A⟧ which still has a left-recursive alternative, split its
 * alternatives into the left-recursive ones, ⟦A → A αᵢ⟧, and the
 * others, ⟦A → βⱼ⟧:
 *
 *     A  → A α₁ | … | A αₘ | β₁ | … | βₙ
 *
 * and replace them with these rules (where ⟦A'⟧ is a new
 * nonterminal):
 *
 *     A  → β₁ A' | … | βₙ A'
 *     A' → α₁ A' | … | αₘ A' | ε
 *
 * Both lists of alternatives must keep the «relative order» of the
 * original alternatives, and ⟦ε⟧ goes last. The name of the new
 * nonterminal is the name of ⟦A⟧ with a ‹'› appended; if that name is
 * already used by a nonterminal (in the original grammar or for
 * another newly created nonterminal), append another ‹'›, and so on.
 * The new nonterminals are created in the order of the original
 * nonterminals (i.e. alphabetically, as given by the ‹std::map›).
 *
 * Nonterminals without immediate left recursion are left unchanged.
 * Notice how the transformation turns a left-recursive rule (which
 * gives left-associative trees) into a right-recursive one (which
 * gives right-associative trees) – fixing that up is the job of the
 * code which builds the tree. */

// grammar remove_left_recursion( const grammar &g );

grammar remove_left_recursion( const grammar &g )
{
    grammar out;

    /* Names in use: the original ones plus the new ones as they are
     * created. Since ‹out› ends up containing all of them, we can
     * simply check both maps. */
    auto fresh = [ & ]( symbol name )
    {
        do
            name += "'";
        while ( g.contains( name ) || out.contains( name ) );
        return name;
    };

    for ( const auto &[ nt, alts ] : g )
    {
        std::vector< sequence > alphas, betas;

        for ( const auto &seq : alts )
            if ( seq.size() == 1 && seq[ 0 ] == nt )
                continue; /* ‹A → A› */
            else if ( !seq.empty() && seq[ 0 ] == nt )
                alphas.emplace_back( seq.begin() + 1, seq.end() );
            else
                betas.push_back( seq );

        if ( alphas.empty() )
        {
            out[ nt ] = betas;
            continue;
        }

        symbol tail = fresh( nt );
        auto &a = out[ nt ];
        auto &t = out[ tail ];

        for ( auto beta : betas )
            beta.push_back( tail ), a.push_back( beta );
        for ( auto alpha : alphas )
            alpha.push_back( tail ), t.push_back( alpha );
        t.emplace_back();
    }

    return out;
}

int main()
{
    grammar expr =
    {
        { "E", { { "E", "+", "T" }, { "E", "-", "T" }, { "T" } } },
        { "T", { { "T", "*", "F" }, { "F" } } },
        { "F", { { "(", "E", ")" }, { "id" } } },
    };

    grammar expr_out =
    {
        { "E",  { { "T", "E'" } } },
        { "E'", { { "+", "T", "E'" }, { "-", "T", "E'" }, {} } },
        { "T",  { { "F", "T'" } } },
        { "T'", { { "*", "F", "T'" }, {} } },
        { "F",  { { "(", "E", ")" }, { "id" } } },
    };

    assert( remove_left_recursion( expr ) == expr_out );
    assert( remove_left_recursion( expr_out ) == expr_out );

    grammar clash =
    {
        { "A",  { { "A", "x" }, { "y" } } },
        { "A'", { { "z" } } },
    };

    grammar clash_out =
    {
        { "A",   { { "y", "A''" } } },
        { "A'",  { { "z" } } },
        { "A''", { { "x", "A''" }, {} } },
    };

    assert( remove_left_recursion( clash ) == clash_out );

    grammar order =
    {
        { "S", { { "a" }, { "S", "b" }, {}, { "S", "c" } } },
    };

    grammar order_out =
    {
        { "S",  { { "a", "S'" }, { "S'" } } },
        { "S'", { { "b", "S'" }, { "c", "S'" }, {} } },
    };

    assert( remove_left_recursion( order ) == order_out );

    grammar trivial = { { "A", { { "A" }, { "a" } } } };
    assert(( remove_left_recursion( trivial ) ==
             grammar{ { "A", { { "a" } } } } ));

    grammar hopeless = { { "A", { { "A", "a" } } } };
    assert(( remove_left_recursion( hopeless ) ==
             grammar{ { "A", {} }, { "A'", { { "a", "A'" }, {} } } } ));

    return 0;
}
