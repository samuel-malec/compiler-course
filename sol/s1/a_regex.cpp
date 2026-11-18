#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <bitset>
#include <memory>
#include <vector>

/* In ‹02/r2_regex›, we matched regular expressions by backtracking,
 * which takes exponential time in the worst case. Most popular regex
 * libraries (in Perl, Python, Java, JavaScript, PCRE, ‹std::regex›)
 * work that way, and real systems have been brought down by
 * carefully chosen patterns and inputs (‘ReDoS’). In this assignment,
 * you will implement a regex engine which «guarantees» running time
 * linear in the length of the input, using the approach of Ken
 * Thompson (1968), which is also used by RE2, Go and Rust:
 *
 *  1. «parse» the pattern into a syntax tree,
 *  2. «compile» the tree into a «nondeterministic finite automaton»
 *     (NFA), with states connected by character transitions and
 *     ‹ε› (empty) transitions,
 *  3. «simulate» the NFA on the input by keeping track of the «set»
 *     of all states the automaton could be in after reading each
 *     character – instead of trying the possibilities one at a time.
 *
 * Since the set of states has at most ⟦m⟧ elements (the size of the
 * NFA, which is linear in the size of the pattern), each character
 * of the input is processed in ⟦O(m)⟧ time, and the whole match takes
 * ⟦O(n · m)⟧. Look up ‘Regular Expression Matching Can Be Simple And
 * Fast’ by Russ Cox for a gentle introduction.
 *
 * The syntax of patterns:
 *
 *     alt    = concat, { '|', concat } ;
 *     concat = { repeat } ;
 *     repeat = atom, { '*' | '+' | '?' | count } ;
 *     count  = '{', int, '}' | '{', int, ',', '}' | '{', int, ',', int, '}' ;
 *     atom   = char | '.' | class | '(', alt, ')' | '\', any ;
 *     class  = '[', [ '^' ], member, { member }, ']' ;
 *     member = cchar, [ '-', cchar ] ;
 *
 * Here, ‹int› is a non-empty sequence of decimal digits, ‹char› is
 * any character except ‹| * + ? { } ( ) [ ] . \›, and ‹any› is any
 * character at all. Inside a class, ‹cchar› is either ‹\› followed by
 * any character (which stands for that character), or any character
 * other than ‹\› and ‹]›, except that a ‹]› which comes «first» in the
 * class (right after ‹[› or ‹[^›) is a literal ‹]›. A ‹-› which is the
 * first or the last character of a class is also literal.
 *
 * The meaning:
 *
 *  • ‹.› matches any single character (including a newline),
 *  • ‹[…]› matches any single character from the set; with ‹^›, any
 *    single character «not» in the set; ‹a-z› is the range of
 *    characters from ‹a› to ‹z› (by their ‹unsigned char› value)
 *    and it is an error if the end is smaller than the start,
 *  • ‹x*›, ‹x+› and ‹x?› are zero or more, one or more and zero or
 *    one repetitions of ‹x›,
 *  • ‹x{m}› is exactly ‹m› repetitions, ‹x{m,}› at least ‹m›, and
 *    ‹x{m,n}› between ‹m› and ‹n› (inclusive); it is an error if
 *    ‹m > n› or if ‹n› (or ‹m›, when there is no ‹n›) exceeds 1000,
 *  • ‹x|y›, concatenation, grouping and escapes as usual; the empty
 *    pattern (or an empty alternative) matches the empty string.
 *
 * Write a class ‹regex› with:
 *
 *  • a constructor which takes the pattern as a ‹std::string_view›
 *    and throws ‹std::invalid_argument› if it is not valid,
 *  • ‹match( text )› – a ‹const› method which returns ‹true› iff the
 *    «entire» ‹text› matches the pattern,
 *  • ‹search( text )› – a ‹const› method which finds the «leftmost»
 *    substring of ‹text› which matches the pattern and, among those
 *    starting at that position, the «longest» one; it returns the
 *    pair (start, length), or ‹std::nullopt› if there is no match.
 *
 * The method ‹match› must run in time ⟦O(n · m)⟧, where ⟦n⟧ is the
 * length of the text and ⟦m⟧ is the size of the pattern «after»
 * counted repetitions are expanded (‹a{3}› is the same as ‹aaa›). It
 * is fine for ‹search› to try each starting position separately
 * (⟦O(n² · m)⟧), but you can also get it down to ⟦O(n · m)⟧ with a
 * little more thought. */

// class regex;

/* ─── syntax tree ─────────────────────────────────────────────────── */

struct rx_node;
using rx_ptr = std::unique_ptr< rx_node >;

struct rx_node
{
    enum kind { set, cat, alt, rep } k;
    std::bitset< 256 > chars = {};           /* for ‹set› */
    std::vector< rx_ptr > kids = {};         /* for ‹cat›, ‹alt› and ‹rep› */
    int min = 0, max = 0;                    /* for ‹rep›; ‹max = -1› is ∞ */
};

struct rx_parser
{
    std::string_view s;
    std::size_t pos = 0;

    [[noreturn]] void fail() { throw std::invalid_argument( "bad regex" ); }
    bool at( char c ) const { return pos < s.size() && s[ pos ] == c; }
    bool at_end() const { return pos >= s.size(); }
    char next() { if ( at_end() ) fail(); return s[ pos++ ]; }

    static rx_ptr make( rx_node::kind k ) { return std::make_unique< rx_node >( k ); }

    rx_ptr alt()
    {
        auto first = concat();
        if ( !at( '|' ) )
            return first;
        auto n = make( rx_node::alt );
        n->kids.push_back( std::move( first ) );
        while ( at( '|' ) )
            ++ pos, n->kids.push_back( concat() );
        return n;
    }

    rx_ptr concat()
    {
        auto n = make( rx_node::cat );
        while ( !at_end() && !at( '|' ) && !at( ')' ) )
            n->kids.push_back( repeat() );
        return n;
    }

    int integer()
    {
        if ( at_end() || s[ pos ] < '0' || s[ pos ] > '9' )
            fail();
        long v = 0;
        while ( !at_end() && s[ pos ] >= '0' && s[ pos ] <= '9' )
            if ( ( v = v * 10 + ( s[ pos++ ] - '0' ) ) > 1000 )
                fail();
        return v;
    }

    rx_ptr repeat()
    {
        auto a = atom();

        while ( at( '*' ) || at( '+' ) || at( '?' ) || at( '{' ) )
        {
            auto r = make( rx_node::rep );
            char c = next();

            if ( c == '*' ) r->min = 0, r->max = -1;
            if ( c == '+' ) r->min = 1, r->max = -1;
            if ( c == '?' ) r->min = 0, r->max = 1;
            if ( c == '{' )
            {
                r->min = r->max = integer();
                if ( at( ',' ) )
                {
                    ++ pos;
                    r->max = at( '}' ) ? -1 : integer();
                }
                if ( next() != '}' || ( r->max >= 0 && r->max < r->min ) )
                    fail();
            }

            r->kids.push_back( std::move( a ) );
            a = std::move( r );
        }

        return a;
    }

    unsigned char cchar()
    {
        char c = next();
        return c == '\\' ? next() : c;
    }

    rx_ptr klass()
    {
        auto n = make( rx_node::set );
        bool negate = at( '^' );
        if ( negate )
            ++ pos;

        bool first = true;
        while ( first || !at( ']' ) )
        {
            if ( at_end() )
                fail();
            first = false;

            unsigned char lo = cchar(), hi = lo;
            if ( at( '-' ) && pos + 1 < s.size() && s[ pos + 1 ] != ']' )
            {
                ++ pos;
                hi = cchar();
                if ( hi < lo )
                    fail();
            }

            for ( unsigned c = lo; c <= hi; ++ c )
                n->chars.set( c );
        }

        ++ pos; /* the closing ‹]› */
        if ( negate )
            n->chars.flip();
        return n;
    }

    rx_ptr atom()
    {
        char c = next();
        auto n = make( rx_node::set );

        switch ( c )
        {
            case '.': n->chars.set(); return n;
            case '[': return klass();
            case '(':
            {
                auto inner = alt();
                if ( next() != ')' )
                    fail();
                return inner;
            }
            case '\\': n->chars.set( static_cast< unsigned char >( next() ) ); return n;
            case '|': case '*': case '+': case '?': case '{': case '}':
            case ')': case ']':
                fail();
            default:
                n->chars.set( static_cast< unsigned char >( c ) );
                return n;
        }
    }
};

/* ─── the NFA ─────────────────────────────────────────────────────── */

/* Each state either consumes a character from ‹chars› and moves to
 * ‹out›, or it is an ‹ε› state with up to two successors, or it is
 * the accepting state. Thompson's construction builds a «fragment»
 * for each node of the tree, with a single entry and a single exit
 * (an ‹ε› state whose successor is filled in later). */

struct nfa_state
{
    bool consumes = false;
    std::bitset< 256 > chars = {};
    int out1 = -1, out2 = -1;
};

struct fragment { int entry, exit; };

class regex
{
    std::vector< nfa_state > _states;
    int _start = 0, _accept = 0;

    int add( nfa_state s ) { _states.push_back( s ); return _states.size() - 1; }
    int eps() { return add( {} ); }

    void link( int from, int to )
    {
        auto &s = _states[ from ];
        ( s.out1 < 0 ? s.out1 : s.out2 ) = to;
    }

    fragment build( const rx_node &n )
    {
        switch ( n.k )
        {
            case rx_node::set:
            {
                int e = eps();
                return { add( { true, n.chars, e, -1 } ), e };
            }

            case rx_node::cat:
            {
                int entry = eps(), exit = entry;
                for ( const auto &k : n.kids )
                {
                    auto f = build( *k );
                    link( exit, f.entry );
                    exit = f.exit;
                }
                return { entry, exit };
            }

            case rx_node::alt:
            {
                /* a chain of binary splits */
                int entry = eps(), exit = eps(), split = entry;
                for ( std::size_t i = 0; i < n.kids.size(); ++ i )
                {
                    auto f = build( *n.kids[ i ] );
                    link( f.exit, exit );
                    link( split, f.entry );
                    if ( i + 1 < n.kids.size() )
                    {
                        int next = eps();
                        link( split, next );
                        split = next;
                    }
                }
                return { entry, exit };
            }

            case rx_node::rep:
            {
                const rx_node &x = *n.kids[ 0 ];
                int entry = eps(), exit = entry;

                auto append = [ & ]( fragment f ) { link( exit, f.entry ); exit = f.exit; };

                for ( int i = 0; i < n.min; ++ i )
                    append( build( x ) );

                if ( n.max < 0 ) /* star: loop back from the exit */
                {
                    auto f = build( x );
                    int loop = eps(), out = eps();
                    link( loop, f.entry );
                    link( loop, out );
                    link( f.exit, loop );
                    link( exit, loop );
                    exit = out;
                }
                else
                    for ( int i = n.min; i < n.max; ++ i ) /* optional copies */
                    {
                        auto f = build( x );
                        int skip = eps(), out = eps();
                        link( skip, f.entry );
                        link( skip, out );
                        link( f.exit, out );
                        link( exit, skip );
                        exit = out;
                    }

                return { entry, exit };
            }
        }

        return {};
    }

    /* The ‹ε›-closure: add ‹s› and everything reachable from it by
     * ‹ε› transitions to ‹set›, unless already there. An explicit
     * stack avoids deep recursion on long ‹ε› chains. Returns the
     * states in the order they were reached. */

    void closure( int s, std::vector< int > &set, std::vector< int > &mark, int gen ) const
    {
        std::vector< int > stack{ s };
        while ( !stack.empty() )
        {
            int t = stack.back();
            stack.pop_back();
            if ( t < 0 || mark[ t ] == gen )
                continue;
            mark[ t ] = gen;
            set.push_back( t );
            if ( !_states[ t ].consumes )
                stack.push_back( _states[ t ].out2 ), stack.push_back( _states[ t ].out1 );
        }
    }

public:
    explicit regex( std::string_view pattern )
    {
        rx_parser p{ pattern };
        auto tree = p.alt();
        if ( !p.at_end() )
            p.fail();

        auto f = build( *tree );
        _start = f.entry;
        _accept = f.exit;
    }

    bool match( std::string_view text ) const
    {
        std::vector< int > cur, next, mark( _states.size(), -1 );
        int gen = 0;
        closure( _start, cur, mark, gen );

        for ( unsigned char c : text )
        {
            next.clear();
            ++ gen;
            for ( int s : cur )
                if ( _states[ s ].consumes && _states[ s ].chars[ c ] )
                    closure( _states[ s ].out1, next, mark, gen );
            std::swap( cur, next );
            if ( cur.empty() )
                return false;
        }

        return mark[ _accept ] == gen;
    }

    /* Leftmost-longest search in a single pass. Every thread (state in
     * the current set) remembers where its match started. Threads are
     * kept in order of increasing start, so when two threads reach
     * the same state, the first one (the one with the earlier start)
     * wins – the later one could never give a better match. Once some
     * match is found, threads which started later are dropped. */

    std::optional< std::pair< std::size_t, std::size_t > > search( std::string_view text ) const
    {
        std::vector< int > cur, next, tmp, mark( _states.size(), -1 );
        std::vector< std::size_t > start( _states.size() );
        std::vector< std::size_t > cur_start, next_start;
        std::optional< std::pair< std::size_t, std::size_t > > best;
        int gen = 0;

        for ( std::size_t i = 0; ; ++ i )
        {
            /* a new thread starting here, with the lowest priority */
            if ( !best )
            {
                tmp.clear();
                closure( _start, tmp, mark, gen );
                for ( int s : tmp )
                    cur.push_back( s ), start[ s ] = i;
            }

            for ( int s : cur )
                if ( s == _accept )
                {
                    std::size_t st = start[ s ];
                    if ( !best || st < best->first ||
                         ( st == best->first && i - st > best->second ) )
                        best = { st, i - st };
                }

            if ( i == text.size() )
                break;

            unsigned char c = text[ i ];
            next.clear();
            ++ gen;

            for ( int s : cur )
            {
                if ( !_states[ s ].consumes || !_states[ s ].chars[ c ] )
                    continue;
                if ( best && start[ s ] > best->first )
                    continue;
                tmp.clear();
                std::size_t st = start[ s ];
                closure( _states[ s ].out1, tmp, mark, gen );
                for ( int t : tmp )
                    next_start.push_back( st ), next.push_back( t );
            }

            for ( std::size_t k = 0; k < next.size(); ++ k )
                start[ next[ k ] ] = next_start[ k ];
            next_start.clear();

            std::swap( cur, next );
            ++ gen; /* states in ‹cur› are not marked for the next round */
            for ( int s : cur )
                mark[ s ] = gen;

            if ( cur.empty() && best )
                break;
        }

        return best;
    }
};

int main()
{
    auto m = []( std::string_view p, std::string_view t ) { return regex( p ).match( t ); };

    assert( m( "", "" ) );
    assert( m( "abc", "abc" ) );
    assert( !m( "abc", "abcd" ) );
    assert( m( "a.c", "a\nc" ) );
    assert( m( "(ab|cd)*e", "abcdabe" ) );
    assert( m( "colou?r", "color" ) );
    assert( m( "[a-c]+x", "abcabcx" ) );
    assert( !m( "[a-c]+x", "abdx" ) );
    assert( m( "[^0-9]*", "abc" ) );
    assert( !m( "[^0-9]*", "ab1" ) );
    assert( m( "[]a]", "]" ) );
    assert( m( "[a-]", "-" ) );
    assert( m( "[\\]]", "]" ) );
    assert( m( "a{3}", "aaa" ) );
    assert( !m( "a{3}", "aa" ) );
    assert( m( "a{2,}", "aaaaa" ) );
    assert( m( "(ab){1,2}", "abab" ) );
    assert( !m( "(ab){1,2}", "ababab" ) );
    assert( m( "a|", "" ) );
    assert( m( "\\(\\)", "()" ) );

    auto throws = []( std::string_view p )
    {
        try { regex r( p ); }
        catch ( const std::invalid_argument & ) { return true; }
        return false;
    };

    assert( throws( "(" ) );
    assert( throws( ")" ) );
    assert( throws( "*" ) );
    assert( throws( "[" ) );
    assert( throws( "[]" ) );
    assert( throws( "[z-a]" ) );
    assert( throws( "a{2,1}" ) );
    assert( throws( "a{1001}" ) );
    assert( throws( "a{,3}" ) );
    assert( throws( "a\\" ) );
    assert( throws( "}" ) );

    using p = std::pair< std::size_t, std::size_t >;
    regex word( "[a-z]+" );
    assert( word.search( "123 hello world" ) == p( 4, 5 ) );
    assert( !word.search( "123 456" ) );
    assert( regex( "a*" ).search( "bbb" ) == p( 0, 0 ) );
    assert( regex( "ab|abcd" ).search( "xabcde" ) == p( 1, 4 ) );

    /* Pathological for backtracking: ‹(a?){n}a{n}› against ‹aⁿ›. */
    std::string pat, text( 30, 'a' );
    for ( int i = 0; i < 30; ++ i ) pat += "a?";
    pat += "a{30}";
    assert( m( pat, text ) );
    assert( !m( pat, text + "b" ) );
    assert( m( "(a*)*b|a*", std::string( 5000, 'a' ) ) );

    return 0;
}
