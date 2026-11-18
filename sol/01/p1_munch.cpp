#include <cassert>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

/* Many languages have a large number of operators made of symbol
 * characters, and the lexer has to decide where one operator ends
 * and the next one begins. Write a function ‹munch› which splits the
 * input into operators taken from the set ‹ops›, using the «maximal
 * munch» rule: at each position, take the «longest» operator from
 * ‹ops› which is a prefix of the remaining input.
 *
 * Spaces separate tokens and are not a part of any operator (the set
 * ‹ops› will never contain a space). If, at some position (other
 * than a space), no operator from ‹ops› matches, return
 * ‹std::nullopt›.
 *
 * Notice that maximal munch never backtracks. Hence it can fail even
 * if the input «could» be split into operators in some other way:
 * with operators ‹-›, ‹->› and ‹>>›, the input ‹->>› is rejected
 * (the lexer first takes ‹->›, and then ‹>› is not an operator),
 * even though ‹- >>› would be a valid split.
 *
 * The number of operators in ‹ops› can be large, so try to avoid
 * looping over all of them at every position. */

// std::optional< std::vector< std::string > >
// munch( std::string_view input, const std::set< std::string > &ops );

/* Instead of trying every operator at every position, we try every
 * «length» from the longest possible down to 1, and look the prefix
 * of that length up in the set. That is ⟦O(k · log n)⟧ per token,
 * where ⟦k⟧ is the length of the longest operator. (A trie would be
 * even better; see the regular exercise ‹dfa› for a related idea.) */

std::optional< std::vector< std::string > >
munch( std::string_view input, const std::set< std::string > &ops )
{
    std::size_t longest = 0;
    for ( const auto &op : ops )
        longest = std::max( longest, op.size() );

    std::vector< std::string > out;
    std::size_t i = 0;

    while ( i < input.size() )
    {
        if ( input[ i ] == ' ' )
        {
            ++ i;
            continue;
        }

        std::size_t len = std::min( longest, input.size() - i );
        for ( ; len > 0; -- len )
            if ( ops.contains( std::string( input.substr( i, len ) ) ) )
                break;

        if ( len == 0 )
            return std::nullopt;

        out.emplace_back( input.substr( i, len ) );
        i += len;
    }

    return out;
}

int main()
{
    using v = std::vector< std::string >;
    using s = std::set< std::string >;

    const s c_ops{ "+", "++", "+=", "-", "--", "-=", "->", "<", "<<",
                   "<<=", "<=", "=", "==", ">", ">=", ">>", ">>=" };

    assert(( munch( "+", c_ops ) == v{ "+" } ));
    assert(( munch( "++", c_ops ) == v{ "++" } ));
    assert(( munch( "+++", c_ops ) == v{ "++", "+" } ));
    assert(( munch( "+++++", c_ops ) == v{ "++", "++", "+" } ));
    assert(( munch( "++ + ++", c_ops ) == v{ "++", "+", "++" } ));
    assert(( munch( "--->", c_ops ) == v{ "--", "->" } ));
    assert(( munch( "<<<=", c_ops ) == v{ "<<", "<=" } ));
    assert(( munch( ">>==", c_ops ) == v{ ">>=", "=" } ));
    assert(( munch( "   ", c_ops ) == v{} ));
    assert(( munch( "", c_ops ) == v{} ));
    assert( !munch( "+!", c_ops ) );

    const s tricky{ "-", "->", ">>" };
    assert(( munch( "- >>", tricky ) == v{ "-", ">>" } ));
    assert( !munch( "->>", tricky ) );

    /* Operators need not be prefix-closed: here, ‹.› is not an
     * operator, but ‹...› is. */
    const s dots{ "...", ".." };
    assert( !munch( "....", dots ) );
    assert(( munch( ".....", dots ) == v{ "...", ".." } ));
    assert(( munch( "......", dots ) == v{ "...", "..." } ));

    return 0;
}
