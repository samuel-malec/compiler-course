#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* Not every language is about functions and statements. «Datalog» is
 * a logic programming language (a subset of Prolog) used for database
 * queries, static program analysis (many pointer analyses are written
 * in it) and access control. A program consists of «facts» and
 * «rules», from which new facts are derived until nothing new can be
 * derived:
 *
 *     parent(alice, bob).
 *     parent(bob, carol).
 *     ancestor(X, Y) :- parent(X, Y).
 *     ancestor(X, Z) :- parent(X, Y), ancestor(Y, Z).
 *     ?- ancestor(alice, W).
 *
 * The last line is a «query», whose answers are ‹W = bob› and
 * ‹W = carol›. Write an interpreter for Datalog with stratified negation.
 *
 * ── Syntax ─────────────────────────────────────────────────────────
 *
 *     program = { clause | query } ;
 *     clause  = atom, [ ':-', literal, { ',', literal } ], '.' ;
 *     query   = '?-', literal, { ',', literal }, '.' ;
 *     literal = atom | 'not', atom | term, cmp, term ;
 *     cmp     = '=' | '!=' | '<' | '<=' ;
 *     atom    = name, [ '(', term, { ',', term }, ')' ] ;
 *     term    = name | integer | variable ;
 *
 * A ‹name› (of a predicate or a constant) starts with a lowercase
 * letter, a ‹variable› with an uppercase letter or ‹_›; both continue
 * with letters, digits and ‹_›. Integers are sequences of digits
 * (constants, too). ‹not› is a keyword. Whitespace separates tokens,
 * and ‹%› starts a comment extending to the end of the line. An atom
 * without parentheses has no arguments (a «propositional» predicate,
 * like ‹raining›).
 *
 * ── Static checks ──────────────────────────────────────────────────
 *
 * Return ‹std::nullopt› if the program is syntactically invalid, or if:
 *
 *  • a predicate is used with different numbers of arguments,
 *  • a clause or query is not «safe»: every variable which appears in
 *    the head of a clause, in a negated atom or in a comparison must
 *    also appear in a positive atom of the same body (so a fact must be
 *    ground); the variable ‹_› is «anonymous» – each occurrence is a
 *    different variable – and it may only appear in positive atoms,
 *  • the program is not «stratifiable»: a predicate depends on itself
 *    through a negation (‹p :- not q.› and ‹q :- p.›).
 *
 * ── Semantics ──────────────────────────────────────────────────────
 *
 * A predicate is «defined» by the clauses with it in the head.
 * Predicates are evaluated in «strata»: a predicate which is used
 * under ‹not› by the clauses of ‹p› must be completely computed before
 * ‹p›. Within a stratum, rules are applied until no new facts appear
 * (the least fixpoint). A positive atom in a body matches the facts
 * of its predicate (binding variables consistently); ‹not a› succeeds
 * if ‹a› (with all its variables bound) is not a fact; ‹=› and ‹!=›
 * compare two constants for equality, ‹<› and ‹<=› compare two integer
 * constants (and are false if either is not an integer). A predicate
 * that is never defined has no facts.
 *
 * Use «semi-naive» evaluation, or at least don't re-derive everything
 * from scratch in a quadratic number of passes – the tests include a
 * transitive closure with ten thousand facts.
 *
 * ── Output ─────────────────────────────────────────────────────────
 *
 * The result is a list of lines: for each query, in order, its
 * answers. If the query has (non-anonymous) variables, each answer is
 * a line ‹X = a, Y = b› giving the values of the variables in the order
 * of their first appearance in the query; the lines of one query are
 * distinct and sorted (as strings); if there are no answers, the
 * single line is ‹false›. A query without variables gives the line
 * ‹true› or ‹false›. */

// std::optional< std::vector< std::string > > run( std::string_view program );

int main()
{
    using v = std::vector< std::string >;

    std::string family =
        "parent(alice, bob). parent(bob, carol). parent(bob, dave). parent(carol, eve).\n"
        "male(bob). male(dave).\n"
        "ancestor(X, Y) :- parent(X, Y).\n"
        "ancestor(X, Z) :- parent(X, Y), ancestor(Y, Z).  % recursion\n"
        "sibling(X, Y) :- parent(P, X), parent(P, Y), X != Y.\n"
        "has_child(X) :- parent(X, _).\n"
        "childless_male(X) :- male(X), not has_child(X).\n";

    assert(( run( family + "?- ancestor(alice, W)." ) == v{ "W = bob", "W = carol", "W = dave", "W = eve" } ));
    assert(( run( family + "?- sibling(A, B)." ) == v{ "A = carol, B = dave", "A = dave, B = carol" } ));
    assert(( run( family + "?- childless_male(X)." ) == v{ "X = dave" } ));
    assert(( run( family + "?- ancestor(bob, eve). ?- ancestor(eve, bob)." ) == v{ "true", "false" } ));
    assert(( run( family + "?- parent(X, Y), ancestor(Y, eve)." ) == v{ "X = alice, Y = bob", "X = bob, Y = carol" } ));
    assert(( run( family + "?- ancestor(nobody, X)." ) == v{ "false" } ));
    assert(( run( family + "?- parent(_, X), not has_child(X)." ) == v{ "X = dave", "X = eve" } ));

    assert(( run( "raining. wet :- raining. dry :- not wet. ?- wet. ?- dry." ) == v{ "true", "false" } ));
    assert(( run( "n(1). n(2). n(3). n(10). small(X) :- n(X), X < 3. ?- small(X). ?- n(X), X <= 2, X = 2." ) ==
             v{ "X = 1", "X = 2", "X = 2" } ));
    assert(( run( "q(a). ?- q(X), X < 5." ) == v{ "false" } ));
    assert(( run( "" ) == v{} ));

    /* a long chain: path has n·(n - 1)/2 facts */
    std::string chain;
    const int n = 150;
    for ( int i = 0; i + 1 < n; ++ i )
        chain += "edge(n" + std::to_string( i ) + ", n" + std::to_string( i + 1 ) + ").\n";
    chain += "path(X, Y) :- edge(X, Y).\n"
             "path(X, Z) :- path(X, Y), edge(Y, Z).\n"
             "?- path(n0, X).\n"
             "?- path(X, n0).\n"
             "?- path(n3, n140).\n";
    auto r = run( chain );
    assert( r && r->size() == std::size_t( n - 1 + 1 + 1 ) );
    assert( ( *r )[ n - 1 ] == "false" && ( *r )[ n ] == "true" );

    assert( !run( "p(X)." ) );                                  /* unsafe fact */
    assert( !run( "p(X) :- q(Y)." ) );                          /* unsafe head */
    assert( !run( "p(X) :- q(X), not r(Y)." ) );                /* unsafe negation */
    assert( !run( "p(X) :- X = 1." ) );                         /* unsafe comparison */
    assert( !run( "p(X) :- q(X), not r(_)." ) );                /* anonymous under not */
    assert( !run( "p(a). p(a, b)." ) );                         /* arity */
    assert( !run( "p :- not q. q :- p." ) );                    /* not stratifiable */
    assert( !run( "p(a)" ) );                                   /* missing ‹.› */
    assert( !run( "P(a)." ) );
    assert( !run( "?- q(X), not r(Y)." ) );

    return 0;
}
