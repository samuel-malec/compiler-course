#include <cassert>
#include <optional>
#include <string_view>
#include <functional>
#include <memory>
#include <vector>

/* Regular expressions are a small language of their own, so they
 * need a parser too. Write a function ‹match( regex, text )› which
 * parses the regular expression and decides whether it matches the
 * «entire» ‹text›. The syntax of regular expressions is:
 *
 *     alt    = concat, { '|', concat } ;
 *     concat = { repeat } ;
 *     repeat = atom, { '*' | '+' | '?' } ;
 *     atom   = char | '.' | '(', alt, ')' | '\', any ;
 *
 * where ‹char› is any character other than ‹| * + ? ( ) . \›, and
 * ‹any› is any character at all. The meaning is as usual: ‹.›
 * matches any single character, ‹\c› matches ‹c› literally, ‹x*›
 * matches zero or more repetitions of ‹x›, ‹x+› one or more and ‹x?›
 * zero or one; ‹x|y› matches either ‹x› or ‹y›; concatenation binds
 * tighter than ‹|›, and the postfix operators bind tighter than
 * concatenation. Notice that ‹concat› can be empty, so ‹a|› and ‹()›
 * are valid (the empty alternative matches the empty string).
 *
 * If the regular expression is not syntactically valid, return
 * ‹std::nullopt›.
 *
 * The suggested approach is to first parse the expression into a
 * tree, and then match the tree against the text using backtracking.
 * A convenient formulation of the matcher is a function which takes
 * a node, a position in the text and a «continuation» (a function
 * which tries to match the rest), and returns whether the node,
 * followed by the continuation, matches. Watch out for repetition of
 * sub-expressions which can match the empty string (like ‹(a*)*›) –
 * a naive implementation will loop forever. The tests are small
 * enough that exponential worst-case behaviour is not a problem
 * here (we will fix that in the assignment set). */

// std::optional< bool > match( std::string_view regex, std::string_view text );

struct rnode;
using rnode_ptr = std::unique_ptr< rnode >;

struct rnode
{
    enum kind { chr, any, cat, alt, star, plus, opt } k;
    char c = 0;
    std::vector< rnode_ptr > kids = {};
};

struct regex_parser
{
    std::string_view s;
    std::size_t pos = 0;
    bool ok = true;

    bool at( char c ) const { return pos < s.size() && s[ pos ] == c; }

    rnode_ptr make( rnode::kind k, char c = 0 )
    {
        return std::make_unique< rnode >( k, c );
    }

    rnode_ptr alt()
    {
        auto first = concat();
        if ( !at( '|' ) )
            return first;

        auto n = make( rnode::alt );
        n->kids.push_back( std::move( first ) );
        while ( at( '|' ) )
        {
            ++ pos;
            n->kids.push_back( concat() );
        }
        return n;
    }

    rnode_ptr concat()
    {
        auto n = make( rnode::cat );
        while ( pos < s.size() && !at( '|' ) && !at( ')' ) )
            n->kids.push_back( repeat() );
        return n;
    }

    rnode_ptr repeat()
    {
        auto a = atom();
        while ( at( '*' ) || at( '+' ) || at( '?' ) )
        {
            char op = s[ pos++ ];
            auto n = make( op == '*' ? rnode::star :
                           op == '+' ? rnode::plus : rnode::opt );
            n->kids.push_back( std::move( a ) );
            a = std::move( n );
        }
        return a;
    }

    rnode_ptr atom()
    {
        char c = s[ pos++ ];

        switch ( c )
        {
            case '.': return make( rnode::any );
            case '(':
            {
                auto inner = alt();
                if ( !at( ')' ) )
                    ok = false;
                ++ pos;
                return inner;
            }
            case '\\':
                if ( pos >= s.size() )
                    return ok = false, make( rnode::any );
                return make( rnode::chr, s[ pos++ ] );
            case '*': case '+': case '?': case ')': case '|':
                ok = false;
                return make( rnode::any );
            default:
                return make( rnode::chr, c );
        }
    }
};

/* The matcher, in continuation-passing style: ‹m( n, i, k )› checks
 * whether ‹n› matches ‹text[ i … j )› for some ‹j› such that ‹k( j )›
 * is true. */

using cont = std::function< bool( std::size_t ) >;

bool m_star( const rnode &x, std::string_view text, std::size_t i, const cont &k );

bool m( const rnode &n, std::string_view text, std::size_t i, const cont &k )
{
    switch ( n.k )
    {
        case rnode::chr:
            return i < text.size() && text[ i ] == n.c && k( i + 1 );
        case rnode::any:
            return i < text.size() && k( i + 1 );
        case rnode::alt:
            for ( const auto &kid : n.kids )
                if ( m( *kid, text, i, k ) )
                    return true;
            return false;
        case rnode::cat:
        {
            std::function< bool( std::size_t, std::size_t ) > step =
                [ & ]( std::size_t idx, std::size_t j )
                {
                    if ( idx == n.kids.size() )
                        return k( j );
                    return m( *n.kids[ idx ], text, j,
                              [ & ]( std::size_t j2 ) { return step( idx + 1, j2 ); } );
                };
            return step( 0, i );
        }
        case rnode::opt:
            return m( *n.kids[ 0 ], text, i, k ) || k( i );
        case rnode::plus:
            /* x+ = x x* */
            return m( *n.kids[ 0 ], text, i, [ & ]( std::size_t j )
            {
                return m_star( *n.kids[ 0 ], text, j, k );
            } );
        case rnode::star:
            return m_star( *n.kids[ 0 ], text, i, k );
    }
    return false;
}

/* ‹x*›: either one more iteration of ‹x› which consumes at least one
 * character (this is what prevents the infinite loop: an iteration
 * which matched nothing would bring us back to the same state), or
 * stop here. */

bool m_star( const rnode &x, std::string_view text, std::size_t i, const cont &k )
{
    auto again = [ & ]( std::size_t j )
    {
        return j > i && m_star( x, text, j, k );
    };
    return m( x, text, i, again ) || k( i );
}

std::optional< bool > match( std::string_view regex, std::string_view text )
{
    regex_parser p{ regex };
    auto root = p.alt();
    if ( !p.ok || p.pos != regex.size() )
        return std::nullopt;

    return m( *root, text, 0, [ & ]( std::size_t j ) { return j == text.size(); } );
}

int main()
{
    assert( match( "", "" ) == true );
    assert( match( "", "a" ) == false );
    assert( match( "abc", "abc" ) == true );
    assert( match( "abc", "ab" ) == false );
    assert( match( "a.c", "axc" ) == true );
    assert( match( "a*", "" ) == true );
    assert( match( "a*", "aaaa" ) == true );
    assert( match( "a+", "" ) == false );
    assert( match( "a+b", "aaab" ) == true );
    assert( match( "colou?r", "color" ) == true );
    assert( match( "colou?r", "colour" ) == true );
    assert( match( "cat|dog", "dog" ) == true );
    assert( match( "cat|dog", "cats" ) == false );
    assert( match( "(ab)*c", "ababc" ) == true );
    assert( match( "(ab)*c", "abac" ) == false );
    assert( match( "a(b|c)*d", "abccbd" ) == true );
    assert( match( "(a|ab)(c|bcd)(d*)", "abcd" ) == true );
    assert( match( "a|", "" ) == true );
    assert( match( "()", "" ) == true );
    assert( match( "(a*)*b", "aaab" ) == true );
    assert( match( "(a*)*b", "aaaaaaaaaa" ) == false );
    assert( match( "(a|b*)*c", "abbac" ) == true );
    assert( match( "a**", "aa" ) == true );
    assert( match( "a?+", "aa" ) == true );
    assert( match( "\\.\\*", ".*" ) == true );
    assert( match( "\\.", "x" ) == false );
    assert( match( ".*x.*", "abxcd" ) == true );

    assert( !match( "(", "" ) );
    assert( !match( "a)", "" ) );
    assert( !match( "*a", "" ) );
    assert( !match( "a|*", "" ) );
    assert( !match( "(*)", "" ) );
    assert( !match( "a\\", "" ) );

    return 0;
}
