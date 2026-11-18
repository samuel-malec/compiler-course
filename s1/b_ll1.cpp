#include <cassert>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/* A «parser generator» takes a grammar and produces a parser for it.
 * In this assignment, you will write one for LL(1) grammars – the
 * grammars for which a recursive descent parser can always decide
 * what to do by looking at a single token. Instead of generating
 * code, your class will interpret the grammar directly, using a
 * «parsing table» and an explicit stack.
 *
 * The grammar is given as text, one or more rules per nonterminal, in
 * this format:
 *
 *     E  -> T E'
 *     E' -> + T E' |
 *     T  -> ( E ) | id
 *
 * Each non-empty line is a rule: a nonterminal (a single word), the
 * arrow ‹->›, and one or more alternatives separated by ‹|›; each
 * alternative is a sequence of symbols separated by spaces, and an
 * empty alternative (or one which consists of the single symbol ‹ε›)
 * stands for the empty sequence. Symbols are arbitrary
 * non-whitespace words (other than ‹->›, ‹|› and ‹ε›). The
 * «nonterminals» are the symbols which appear on the left of some
 * rule (a nonterminal can have several lines; their alternatives are
 * collected in order), all other symbols are «terminals». The
 * nonterminal of the first rule is the «start symbol». The word ‹$›
 * is reserved for the end of input and must not appear in the
 * grammar. If a line is malformed (no arrow, or not exactly one word
 * before it) or the grammar has no rules at all, the constructor
 * throws ‹std::invalid_argument›.
 *
 * The class ‹ll1› must provide these (‹const›) methods:
 *
 *  • ‹first( A )› – the FIRST set of the nonterminal ‹A› (as in
 *    ‹02/p4_first›), including the empty string if ‹A› is nullable,
 *  • ‹follow( A )› – the FOLLOW set of ‹A›: the set of terminals which
 *    can appear immediately after ‹A› in some sentential form derived
 *    from the start symbol, plus ‹$› if ‹A› can appear at the very end
 *    (in particular, ‹$› is in the FOLLOW set of the start symbol),
 *  • ‹is_ll1()› – whether the grammar is LL(1),
 *  • ‹parse( tokens )› – parse a sequence of terminals.
 *
 * Both ‹first› and ‹follow› throw ‹std::invalid_argument› if ‹A› is
 * not a nonterminal. FOLLOW sets are again computed as a fixpoint: for
 * every rule ⟦A → α B β⟧, everything in ⟦FIRST(β)⟧ (except ε) is in
 * ⟦FOLLOW(B)⟧, and if ⟦β⟧ is nullable, then also everything in
 * ⟦FOLLOW(A)⟧ is.
 *
 * The «predict set» of an alternative ⟦A → α⟧ is ⟦FIRST(α)⟧ (without
 * ε), plus ⟦FOLLOW(A)⟧ if ⟦α⟧ is nullable. The grammar is LL(1) if, for
 * each nonterminal, the predict sets of its alternatives are
 * pairwise disjoint. The parser then works as follows: it keeps a
 * stack of symbols, initially just the start symbol. If the top of
 * the stack is a terminal, it must be equal to the next token, and
 * both are removed. If it is a nonterminal ⟦A⟧, the parser looks at
 * the next token (‹$› at the end of input), finds the alternative of
 * ⟦A⟧ whose predict set contains it, and replaces ⟦A⟧ on the stack by
 * the symbols of that alternative. The input is accepted when both
 * the stack and the input are empty.
 *
 * The result of ‹parse› is the «parse tree», written as an
 * S-expression: a nonterminal node is ‹(A c₁ c₂ …)›, where ‹cᵢ› are its
 * children (so a node for an empty alternative is just ‹(A)›), and a
 * terminal is written as itself. If the input is not in the language,
 * the result is ‹std::nullopt›. If the grammar is not LL(1), ‹parse›
 * throws ‹std::logic_error›. Since the parser uses an explicit stack,
 * it should cope with long inputs (the tests use one with a hundred
 * thousand tokens). */

// class ll1;

int main()
{
    using s = std::set< std::string >;
    using v = std::vector< std::string >;

    ll1 expr( "E  -> T E'\n"
              "E' -> + T E' |\n"
              "T  -> F T'\n"
              "T' -> * F T' | ε\n"
              "F  -> ( E ) | id\n" );

    assert(( expr.first( "E" ) == s{ "(", "id" } ));
    assert(( expr.first( "E'" ) == s{ "+", "" } ));
    assert(( expr.follow( "E" ) == s{ ")", "$" } ));
    assert(( expr.follow( "E'" ) == s{ ")", "$" } ));
    assert(( expr.follow( "T" ) == s{ "+", ")", "$" } ));
    assert(( expr.follow( "F" ) == s{ "*", "+", ")", "$" } ));
    assert( expr.is_ll1() );

    assert( expr.parse( { "id" } ) == "(E (T (F id) (T')) (E'))" );
    assert( expr.parse( { "id", "+", "id", "*", "id" } ) ==
            "(E (T (F id) (T')) (E' + (T (F id) (T' * (F id) (T'))) (E')))" );
    assert( expr.parse( { "(", "id", ")" } ) ==
            "(E (T (F ( (E (T (F id) (T')) (E')) )) (T')) (E'))" );
    assert( !expr.parse( {} ) );
    assert( !expr.parse( { "id", "+" } ) );
    assert( !expr.parse( { "(", "id" } ) );
    assert( !expr.parse( { "id", "id" } ) );

    /* Left recursion is never LL(1). */
    ll1 leftrec( "E -> E + T | T\nT -> id" );
    assert( !leftrec.is_ll1() );
    assert(( leftrec.first( "E" ) == s{ "id" } ));

    bool threw = false;
    try { leftrec.parse( { "id" } ); } catch ( const std::logic_error & ) { threw = true; }
    assert( threw );

    /* A FIRST/FOLLOW conflict: after ‹A›, an ‹a› could belong either
     * to ‹A› or to whatever follows it. */
    ll1 ff( "S -> A a\nA -> a |" );
    assert( !ff.is_ll1() );
    assert(( ff.follow( "A" ) == s{ "a" } ));

    /* Rules spread over several lines, and a long input. */
    ll1 list( "L -> x R\nR -> , x R\nR -> ε" );
    assert( list.is_ll1() );
    assert( list.parse( { "x", ",", "x" } ) == "(L x (R , x (R)))" );

    v tokens{ "x" };
    for ( int i = 0; i < 50000; ++ i )
        tokens.push_back( "," ), tokens.push_back( "x" );
    auto tree = list.parse( tokens );
    assert( tree && tree->size() == 9 + 50000 * 8 );

    auto bad = []( std::string_view g )
    {
        try { ll1 x( g ); }
        catch ( const std::invalid_argument & ) { return true; }
        return false;
    };

    assert( bad( "" ) );
    assert( bad( "A B -> c" ) );
    assert( bad( "A c" ) );
    assert( bad( "-> c" ) );
    assert( bad( "A -> $" ) );

    return 0;
}
