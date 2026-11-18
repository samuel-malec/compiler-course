#include <cassert>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <sstream>

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

class ll1
{
    using symbol = std::string;
    using sequence = std::vector< symbol >;

    std::map< symbol, std::vector< sequence > > _rules;
    symbol _start;
    std::map< symbol, std::set< symbol > > _first, _follow;
    std::map< symbol, std::map< symbol, std::size_t > > _table; /* A → token → alternative */
    bool _ll1 = true;

    bool is_nt( const symbol &x ) const { return _rules.contains( x ); }

    /* FIRST of a sequence (with ‹""› if the whole sequence is nullable). */
    std::set< symbol > first_of( const sequence &seq, std::size_t from = 0 ) const
    {
        std::set< symbol > out;
        for ( std::size_t i = from; i < seq.size(); ++ i )
        {
            std::set< symbol > f = is_nt( seq[ i ] ) ? _first.at( seq[ i ] )
                                                     : std::set< symbol >{ seq[ i ] };
            bool nullable = f.erase( "" ) > 0;
            out.insert( f.begin(), f.end() );
            if ( !nullable )
                return out;
        }
        out.insert( "" );
        return out;
    }

    void parse_grammar( std::string_view text )
    {
        std::istringstream lines{ std::string( text ) };

        for ( std::string line; std::getline( lines, line ); )
        {
            std::istringstream words( line );
            std::vector< std::string > w;
            for ( std::string x; words >> x; )
                w.push_back( x );

            if ( w.empty() )
                continue;
            if ( w.size() < 2 || w[ 1 ] != "->" || w[ 0 ] == "|" || w[ 0 ] == "ε" )
                throw std::invalid_argument( "malformed rule" );

            if ( _start.empty() )
                _start = w[ 0 ];

            auto &alts = _rules[ w[ 0 ] ];
            sequence current;
            auto finish = [ & ]
            {
                if ( current.size() == 1 && current[ 0 ] == "ε" )
                    current.clear();
                alts.push_back( current );
                current.clear();
            };

            for ( std::size_t i = 2; i < w.size(); ++ i )
                if ( w[ i ] == "|" )
                    finish();
                else if ( w[ i ] == "->" || w[ i ] == "$" )
                    throw std::invalid_argument( "reserved symbol" );
                else
                    current.push_back( w[ i ] );
            finish();
        }

        if ( _rules.empty() )
            throw std::invalid_argument( "empty grammar" );
    }

    void compute_first()
    {
        for ( bool changed = true; changed; )
        {
            changed = false;
            for ( const auto &[ a, alts ] : _rules )
                for ( const auto &seq : alts )
                {
                    auto f = first_of( seq );
                    auto &target = _first[ a ];
                    auto before = target.size();
                    target.insert( f.begin(), f.end() );
                    changed = changed || target.size() != before;
                }
        }
    }

    void compute_follow()
    {
        _follow[ _start ].insert( "$" );

        for ( bool changed = true; changed; )
        {
            changed = false;
            for ( const auto &[ a, alts ] : _rules )
                for ( const auto &seq : alts )
                    for ( std::size_t i = 0; i < seq.size(); ++ i )
                    {
                        if ( !is_nt( seq[ i ] ) )
                            continue;

                        auto &target = _follow[ seq[ i ] ];
                        auto before = target.size();
                        auto rest = first_of( seq, i + 1 );

                        if ( rest.erase( "" ) )
                        {
                            const auto &fa = _follow[ a ];
                            target.insert( fa.begin(), fa.end() );
                        }
                        target.insert( rest.begin(), rest.end() );
                        changed = changed || target.size() != before;
                    }
        }
    }

    void build_table()
    {
        for ( const auto &[ a, alts ] : _rules )
            for ( std::size_t i = 0; i < alts.size(); ++ i )
            {
                auto predict = first_of( alts[ i ] );
                if ( predict.erase( "" ) )
                {
                    const auto &fa = _follow[ a ];
                    predict.insert( fa.begin(), fa.end() );
                }

                for ( const auto &t : predict )
                    if ( !_table[ a ].emplace( t, i ).second )
                        _ll1 = false;
            }
    }

public:
    explicit ll1( std::string_view grammar )
    {
        parse_grammar( grammar );
        for ( const auto &[ a, _ ] : _rules )
            _first[ a ], _follow[ a ];
        compute_first();
        compute_follow();
        build_table();
    }

    std::set< symbol > first( const symbol &a ) const
    {
        if ( !is_nt( a ) ) throw std::invalid_argument( "not a nonterminal" );
        return _first.at( a );
    }

    std::set< symbol > follow( const symbol &a ) const
    {
        if ( !is_nt( a ) ) throw std::invalid_argument( "not a nonterminal" );
        return _follow.at( a );
    }

    bool is_ll1() const { return _ll1; }

    /* The stack holds symbols to match; the empty string is a marker
     * which closes the parenthesis of a finished nonterminal node. */

    std::optional< std::string > parse( const std::vector< symbol > &tokens ) const
    {
        if ( !_ll1 )
            throw std::logic_error( "the grammar is not LL(1)" );

        std::vector< symbol > stack{ _start };
        std::string out;
        std::size_t pos = 0;

        /* Every item except the very first is preceded by a space;
         * closing parentheses are appended directly. */
        auto emit = [ & ]( const std::string &s )
        {
            if ( !out.empty() )
                out += ' ';
            out += s;
        };

        while ( !stack.empty() )
        {
            symbol top = stack.back();
            stack.pop_back();

            if ( top.empty() )
            {
                out += ')';
                continue;
            }

            const symbol &next = pos < tokens.size() ? tokens[ pos ] : "$";

            if ( !is_nt( top ) )
            {
                if ( top != next || pos >= tokens.size() )
                    return std::nullopt;
                emit( top );
                ++ pos;
                continue;
            }

            auto row = _table.find( top );
            if ( row == _table.end() )
                return std::nullopt;
            auto cell = row->second.find( next );
            if ( cell == row->second.end() )
                return std::nullopt;

            emit( "(" + top );
            stack.push_back( "" );
            const auto &alt = _rules.at( top )[ cell->second ];
            for ( auto it = alt.rbegin(); it != alt.rend(); ++ it )
                stack.push_back( *it );
        }

        if ( pos != tokens.size() )
            return std::nullopt;
        return out;
    }
};

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
