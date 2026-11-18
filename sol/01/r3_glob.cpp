#include <cassert>
#include <string>
#include <string_view>

/* Shell «glob» patterns (as in ‹ls *.cpp›) are a small pattern
 * language of their own, and matching them is a nice warm-up for the
 * automata-based matching that real lexers do. Write a function
 * ‹glob( pattern, text )› which decides whether the «entire» ‹text›
 * matches the ‹pattern›, where in the pattern:
 *
 *  • ‹?› matches any single character,
 *  • ‹*› matches any sequence of characters (including an empty one),
 *  • ‹[…]› is a «character class» which matches a single character
 *    from the given set; the set is a sequence of single characters
 *    and ranges like ‹a-z›; if the first character after ‹[› is ‹!›,
 *    the class is negated (matches any character «not» in the set);
 *    the class ends with the first ‹]› which is not the very first
 *    character of the set (so ‹[]a]› matches ‹]› or ‹a›),
 *  • ‹\c› matches the character ‹c› literally (for any ‹c›),
 *  • any other character matches itself.
 *
 * You can assume that the pattern is well-formed (every ‹[› has a
 * matching ‹]› and the pattern does not end with a lone ‹\›).
 *
 * Be careful about efficiency: a naive backtracking matcher takes
 * exponential time on patterns like ‹*a*a*a*a*b›. Your solution
 * should run in time ⟦O(m·n)⟧, where ⟦m⟧ and ⟦n⟧ are the lengths of
 * the pattern and the text. One way to get there is to «remember»
 * the results of sub-problems ‘does the pattern suffix starting at
 * ⟦i⟧ match the text suffix starting at ⟦j⟧’; there is also a neat
 * greedy algorithm which only needs to remember the position of the
 * last ‹*›. */

// bool glob( std::string_view pattern, std::string_view text );

/* Try to match a single pattern element (not ‹*›) at ‹pattern[ i ]›
 * against the character ‹c›. Returns the index just past the element
 * on success, or ‹npos› on failure. */

std::size_t match_one( std::string_view pattern, std::size_t i, char c )
{
    const auto fail = std::string_view::npos;

    switch ( pattern[ i ] )
    {
        case '?':  return i + 1;
        case '\\': return pattern[ i + 1 ] == c ? i + 2 : fail;
        case '[':
        {
            std::size_t j = i + 1;
            bool negate = pattern[ j ] == '!';
            if ( negate )
                ++ j;

            bool found = false;
            std::size_t first = j;

            for ( ; j == first || pattern[ j ] != ']'; ++ j )
                if ( j + 2 < pattern.size() && pattern[ j + 1 ] == '-' &&
                     pattern[ j + 2 ] != ']' )
                {
                    found = found || ( c >= pattern[ j ] && c <= pattern[ j + 2 ] );
                    j += 2;
                }
                else
                    found = found || c == pattern[ j ];

            return found != negate ? j + 1 : fail;
        }
        default:
            return pattern[ i ] == c ? i + 1 : fail;
    }
}

/* The greedy algorithm: walk the pattern and the text together. When
 * we see a ‹*›, remember where it was and let it match nothing for
 * now. When we get stuck, go back to the last ‹*› and let it eat one
 * more character. We never need to go back to an «earlier» star: if
 * the part of the pattern between the two stars matched somewhere,
 * the later star can absorb whatever the earlier star would have.
 * Each ‘go back’ moves the star's position in the text forward, so
 * the total work is ⟦O(m·n)⟧. */

bool glob( std::string_view pattern, std::string_view text )
{
    const auto npos = std::string_view::npos;
    std::size_t p = 0, t = 0;
    std::size_t star_p = npos, star_t = 0;

    while ( t < text.size() )
    {
        if ( p < pattern.size() && pattern[ p ] == '*' )
        {
            star_p = ++ p;
            star_t = t;
            continue;
        }

        if ( p < pattern.size() )
            if ( auto next = match_one( pattern, p, text[ t ] ); next != npos )
            {
                p = next;
                ++ t;
                continue;
            }

        if ( star_p == npos )
            return false;

        p = star_p;
        t = ++ star_t;
    }

    while ( p < pattern.size() && pattern[ p ] == '*' )
        ++ p;

    return p == pattern.size();
}

int main()
{
    assert( glob( "", "" ) );
    assert( !glob( "", "a" ) );
    assert( glob( "abc", "abc" ) );
    assert( !glob( "abc", "abd" ) );
    assert( glob( "a?c", "abc" ) );
    assert( !glob( "a?c", "ac" ) );
    assert( glob( "*", "" ) );
    assert( glob( "*", "anything" ) );
    assert( glob( "*.cpp", "main.cpp" ) );
    assert( !glob( "*.cpp", "main.cpp.bak" ) );
    assert( glob( "a*b*c", "aXXbYYc" ) );
    assert( glob( "a*b*c", "abc" ) );
    assert( !glob( "a*b*c", "acb" ) );
    assert( glob( "[abc]x", "bx" ) );
    assert( !glob( "[abc]x", "dx" ) );
    assert( glob( "[a-cx-z]", "y" ) );
    assert( !glob( "[a-cx-z]", "d" ) );
    assert( glob( "[!a-c]", "d" ) );
    assert( !glob( "[!a-c]", "b" ) );
    assert( glob( "[]a]", "]" ) );
    assert( glob( "[!]]", "x" ) );
    assert( !glob( "[!]]", "]" ) );
    assert( glob( "\\*", "*" ) );
    assert( !glob( "\\*", "a" ) );
    assert( glob( "\\[a]", "[a]" ) );
    assert( glob( "d?_*.[ch]pp", "d1_pipeline.cpp" ) );

    /* Pathological for naive backtracking. */
    std::string pat, text( 2000, 'a' );
    for ( int i = 0; i < 30; ++ i )
        pat += "*a";
    assert( glob( pat, text ) );
    assert( !glob( pat + "*b", text ) );

    return 0;
}
