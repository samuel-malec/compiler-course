#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <set>

/* The grammar of Go (like that of C) uses semicolons to terminate
 * statements, but Go programmers almost never write them. Instead,
 * the «lexer» inserts them automatically, using a simple rule based
 * only on the last token of each line. This makes the job of the
 * parser easy (it sees the semicolons as usual) and keeps the
 * language free of semicolon clutter.
 *
 * Write a function ‹semi› which lexes a Go-like language and
 * performs semicolon insertion. The tokens of the language are:
 *
 *  • identifiers – a letter or ‹_› followed by letters, digits and
 *    ‹_›; this includes the keywords (see below), which are lexically
 *    just identifiers,
 *  • integers – a sequence of decimal digits,
 *  • the operators ‹++› and ‹--›,
 *  • single-character symbols: ‹+ - * / = < > ( ) { } , ;›.
 *
 * Whitespace (spaces, tabs and newlines) separates tokens, and ‹//›
 * starts a comment which extends to the end of the line.
 *
 * The insertion rule: when the lexer reaches a «newline» or the
 * «end of input», it inserts a ‹;› token if the last token before it
 * (on the same line) was one of:
 *
 *  • an identifier which is not a keyword, or one of the keywords
 *    ‹return›, ‹break› and ‹continue›,
 *  • an integer,
 *  • one of ‹++›, ‹--›, ‹)› or ‹}›.
 *
 * The keywords are ‹break›, ‹continue›, ‹else›, ‹for›, ‹func›, ‹if›,
 * ‹return› and ‹var›. Notice that a line which ends with a comment
 * is still subject to the rule – the comment is not a token.
 *
 * The result is the list of lexemes, including the inserted ‹;›.
 * Return ‹std::nullopt› if the input contains a character which
 * cannot start a token. */

// std::optional< std::vector< std::string > > semi( std::string_view src );

bool is_digit( char c ) { return c >= '0' && c <= '9'; }
bool is_alpha( char c )
{
    return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_';
}

/* Decide whether a line ending after token ‹t› gets a semicolon. */

bool triggers( const std::string &t )
{
    static const std::set< std::string > keywords =
        { "break", "continue", "else", "for", "func", "if", "return", "var" };
    static const std::set< std::string > enders =
        { "return", "break", "continue", "++", "--", ")", "}" };

    if ( enders.contains( t ) )
        return true;
    if ( is_alpha( t[ 0 ] ) )
        return !keywords.contains( t );
    return is_digit( t[ 0 ] );
}

std::optional< std::vector< std::string > > semi( std::string_view src )
{
    std::vector< std::string > out;
    bool line_has_token = false; /* did the current line produce a token? */
    std::size_t i = 0;

    auto end_of_line = [ & ]
    {
        if ( line_has_token && triggers( out.back() ) )
            out.emplace_back( ";" );
        line_has_token = false;
    };

    while ( i < src.size() )
    {
        char c = src[ i ];
        std::size_t start = i++;

        if ( c == '\n' )
        {
            end_of_line();
            continue;
        }

        if ( c == ' ' || c == '\t' )
            continue;

        if ( c == '/' && i < src.size() && src[ i ] == '/' )
        {
            while ( i < src.size() && src[ i ] != '\n' )
                ++ i;
            continue;
        }

        if ( is_alpha( c ) )
            while ( i < src.size() && ( is_alpha( src[ i ] ) || is_digit( src[ i ] ) ) )
                ++ i;
        else if ( is_digit( c ) )
            while ( i < src.size() && is_digit( src[ i ] ) )
                ++ i;
        else if ( ( c == '+' || c == '-' ) && i < src.size() && src[ i ] == c )
            ++ i;
        else if ( std::string_view( "+-*/=<>(){},;" ).find( c ) == std::string_view::npos )
            return std::nullopt;

        out.emplace_back( src.substr( start, i - start ) );
        line_has_token = true;
    }

    end_of_line();
    return out;
}

int main()
{
    using v = std::vector< std::string >;

    assert(( semi( "" ) == v{} ));
    assert(( semi( "\n\n" ) == v{} ));
    assert(( semi( "x = 1\ny = 2\n" ) ==
             v{ "x", "=", "1", ";", "y", "=", "2", ";" } ));
    assert(( semi( "x" ) == v{ "x", ";" } ));
    assert(( semi( "x;\n" ) == v{ "x", ";" } ));
    assert(( semi( "func f() {\n\treturn\n}\n" ) ==
             v{ "func", "f", "(", ")", "{", "return", ";", "}", ";" } ));
    assert(( semi( "if x {\n  y++\n} else {\n  y--\n}" ) ==
             v{ "if", "x", "{", "y", "++", ";", "}", "else", "{",
                "y", "--", ";", "}", ";" } ));
    assert(( semi( "a +\n  b" ) == v{ "a", "+", "b", ";" } ));
    assert(( semi( "f(a,\n  b)\n" ) == v{ "f", "(", "a", ",", "b", ")", ";" } ));
    assert(( semi( "x // comment\ny" ) == v{ "x", ";", "y", ";" } ));
    assert(( semi( "for\nelse\nvar\n" ) == v{ "for", "else", "var" } ));
    assert(( semi( "break\ncontinue" ) == v{ "break", ";", "continue", ";" } ));
    assert(( semi( "a+++b--\n" ) == v{ "a", "++", "+", "b", "--", ";" } ));
    assert(( semi( "x / y\n// only a comment\n" ) == v{ "x", "/", "y", ";" } ));
    assert(( semi( "forx elsey\n" ) == v{ "forx", "elsey", ";" } ));

    assert( !semi( "x $ y" ) );
    assert( !semi( "\"str\"" ) );

    return 0;
}
