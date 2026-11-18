#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/* The substitution in ‹d1_unify› is ‘triangular’: to find what a
 * variable stands for, we may have to follow a long chain of bindings
 * ‹'a ↦ 'b ↦ 'c ↦ …›, and we do that again on every lookup. On large
 * inputs, inference then takes quadratic time. Real implementations
 * use a «union-find» structure instead: each type variable is a node
 * which either is unbound, or points to another node or to a type;
 * lookups follow the pointers and then «compress» the path (make every
 * node on it point directly to the end), and binding a variable to
 * another variable links their «roots» (the smaller tree under the
 * larger one, which keeps the trees shallow).
 *
 * Write a function ‹solve› which takes a list of «equations» between
 * types (each side given as text, in the notation of this chapter)
 * and a list of type variables, solves the equations by unification,
 * and returns, for each of the given variables, the type it is bound
 * to, with all variables resolved as far as possible (written as an
 * S-expression, with spaces between elements; variables which remain
 * unbound keep their original names – when two unbound variables are
 * unified, either of them may remain). Return ‹std::nullopt› if the
 * equations have no solution (including the occurs check).
 *
 * The tests include a chain of a hundred thousand equations, given in
 * an order which makes naive chained substitution take quadratic
 * time. Your solution should be close to linear. The types in the
 * results are small, so you don't need to worry about the time it
 * takes to write them out. You will need to parse the types yourself
 * (there is no ‹read› in this file); they consist of atoms and lists,
 * as usual. */

// std::optional< std::vector< std::string > >
// solve( const std::vector< std::pair< std::string, std::string > > &equations,
//        const std::vector< std::string > &query );

int main()
{
    using eqs = std::vector< std::pair< std::string, std::string > >;
    using v = std::vector< std::string >;

    assert(( solve( { { "'a", "int" } }, { "'a" } ) == v{ "int" } ));
    assert(( solve( { { "'a", "(-> 'b int)" }, { "'b", "bool" } }, { "'a", "'b" } ) ==
             v{ "(-> bool int)", "bool" } ));
    assert(( solve( { { "(-> 'a 'b)", "(-> 'b (list 'c))" }, { "'c", "int" } }, { "'a" } ) ==
             v{ "(list int)" } ));
    assert(( solve( { { "'a", "'a" } }, { "'a" } ) == v{ "'a" } ));
    assert(( solve( { { "'x", "(pair 'y 'y)" }, { "'y", "(list 'z)" } }, { "'x", "'q" } ) ==
             v{ "(pair (list 'z) (list 'z))", "'q" } ));

    auto r = solve( { { "'a", "'b" } }, { "'a", "'b" } );
    assert( r && ( *r == v{ "'a", "'a" } || *r == v{ "'b", "'b" } ) );

    assert( !solve( { { "int", "bool" } }, {} ) );
    assert( !solve( { { "'a", "(-> 'a int)" } }, {} ) );
    assert( !solve( { { "'a", "(-> 'b int)" }, { "'b", "(list 'a)" } }, {} ) );
    assert( !solve( { { "(pair 'a 'b)", "(-> 'a 'b)" } }, {} ) );

    /* a long chain, linked backwards: 'v1 = 'v0, 'v2 = 'v1, … */
    const int n = 100000;
    eqs chain;
    for ( int i = 1; i <= n; ++ i )
        chain.emplace_back( "'v" + std::to_string( i ), "'v" + std::to_string( i - 1 ) );
    chain.emplace_back( "'v0", "(-> int bool)" );
    v query, expect;
    for ( int i = 0; i <= n; i += 1000 )
        query.push_back( "'v" + std::to_string( i ) ), expect.push_back( "(-> int bool)" );
    assert( solve( chain, query ) == expect );

    return 0;
}
