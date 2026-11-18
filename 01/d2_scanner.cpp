#include <cassert>
#include <string_view>
#include <vector>
#include <map>

/* In this demo, we will write a proper lexer for the first version
 * of the language ‹tiny›, which we will keep extending throughout
 * the course. A short example of a ‹tiny› program:
 *
 *     # the answer
 *     let x = 6 in
 *     let y = 7 in
 *         if x * y >= 42 then "yes" else "no"
 *
 * The tokens of ‹tiny› (version 1) are:
 *
 *  • «identifiers» – a letter or ‹_›, followed by any number of
 *    letters, digits and ‹_›,
 *  • «keywords» – ‹let›, ‹in›, ‹if›, ‹then›, ‹else›, ‹true›,
 *    ‹false›, ‹and›, ‹or› and ‹not›,
 *  • «integer literals» – a non-empty sequence of digits,
 *  • «string literals» – any characters except ‹"› and a newline,
 *    enclosed in double quotes (no escapes, yet – see ‹p2_strlit›),
 *  • «operators» – ‹+ - * / % = == != < <= > >=›,
 *  • «punctuation» – ‹(› and ‹)›.
 *
 * Whitespace separates tokens and is otherwise ignored, and so are
 * «comments», which start with ‹#› and extend to the end of the
 * line. */

enum class tok
{
    identifier, keyword, integer, string, op, lparen, rparen,
    error, end
};

/* Unlike in ‹d1_pipeline›, a token remembers its «lexeme» – the
 * slice of the input it was made from. Since we use
 * ‹std::string_view› both for the input and for lexemes, slicing is
 * cheap: no characters are copied. The price is that tokens must not
 * outlive the input string. We also remember the offset at which the
 * token starts (for error messages; see ‹d3_errors›). */

struct token
{
    tok kind;
    std::string_view text;
    std::size_t offset;
};

/* A lexer is basically a «cursor» – a position in the input – with a
 * few helper methods for looking at and consuming characters. The
 * helpers are tiny, but they make the actual lexing code much easier
 * to read, and they take care of the bounds checks in one place: if
 * we try to look past the end of input, we get a NUL character
 * instead. */

struct lexer
{
    std::string_view src;
    std::size_t pos = 0;

    bool at_end() const { return pos >= src.size(); }
    char peek( std::size_t ahead = 0 ) const
    {
        return pos + ahead < src.size() ? src[ pos + ahead ] : '\0';
    }

    char advance() { return src[ pos++ ]; }

    /* Consume the next character if it is ‹c›; this is the bread and
     * butter of maximal munch for operators. */
    bool match( char c )
    {
        if ( at_end() || src[ pos ] != c )
            return false;
        ++ pos;
        return true;
    }

    static bool is_digit( char c ) { return c >= '0' && c <= '9'; }
    static bool is_alpha( char c )
    {
        return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
               c == '_';
    }

    /* Skip any mix of whitespace and comments. Note the loop: a
     * comment can be followed by more whitespace, which can be
     * followed by another comment, and so on. */

    void skip()
    {
        while ( !at_end() )
        {
            char c = peek();

            if ( c == ' ' || c == '\t' || c == '\n' || c == '\r' )
                advance();
            else if ( c == '#' )
                while ( !at_end() && peek() != '\n' )
                    advance();
            else
                break;
        }
    }

    /* A helper to build a token which spans from ‹start› to the
     * current position. */

    token make( tok kind, std::size_t start ) const
    {
        return { kind, src.substr( start, pos - start ), start };
    }

    /* The main function of the lexer: produce the next token. This
     * is where the lexer decides, based on the first character, which
     * kind of token to read. */

    token next()
    {
        skip();
        std::size_t start = pos;

        if ( at_end() )
            return make( tok::end, start );

        char c = advance();

        if ( is_alpha( c ) )
            return identifier( start );

        if ( is_digit( c ) )
        {
            while ( is_digit( peek() ) )
                advance();
            return make( tok::integer, start );
        }

        if ( c == '"' )
            return string( start );

        switch ( c )
        {
            case '(': return make( tok::lparen, start );
            case ')': return make( tok::rparen, start );
            case '+': case '-': case '*': case '/': case '%':
                return make( tok::op, start );

            /* Maximal munch for two-character operators: if the
             * next character is ‹=›, it belongs to this token.
             * Notice that ‹!› alone is not a valid operator. */
            case '=': case '<': case '>':
                match( '=' );
                return make( tok::op, start );
            case '!':
                return make( match( '=' ) ? tok::op : tok::error, start );
        }

        return make( tok::error, start );
    }

    /* Identifiers and keywords share the same syntax, so we first
     * read the longest identifier (maximal munch again: ‹iffy› is an
     * identifier, not the keyword ‹if› followed by ‹fy›), and then
     * check whether it happens to be a keyword. */

    token identifier( std::size_t start )
    {
        while ( is_alpha( peek() ) || is_digit( peek() ) )
            advance();

        static const std::string_view keywords[] =
        {
            "let", "in", "if", "then", "else",
            "true", "false", "and", "or", "not"
        };

        token t = make( tok::identifier, start );
        for ( auto kw : keywords )
            if ( t.text == kw )
                t.kind = tok::keyword;
        return t;
    }

    /* A string literal: read until the closing quote. If we hit a
     * newline or the end of input first, the literal is unterminated,
     * which is an error. The lexeme includes the quotes. */

    token string( std::size_t start )
    {
        while ( !at_end() && peek() != '"' && peek() != '\n' )
            advance();

        if ( !match( '"' ) )
            return make( tok::error, start );

        return make( tok::string, start );
    }
};

/* Finally, a convenience function which lexes the whole input into a
 * vector. It stops after the first ‹end› token. Error tokens do «not»
 * stop the lexer: it simply continues after the offending character.
 */

std::vector< token > lex( std::string_view src )
{
    lexer l{ src };
    std::vector< token > out;

    do
        out.push_back( l.next() );
    while ( out.back().kind != tok::end );

    return out;
}

int main() /* demo */
{
    auto kinds = []( std::string_view src )
    {
        std::vector< tok > out;
        for ( auto t : lex( src ) )
            out.push_back( t.kind );
        return out;
    };

    auto texts = []( std::string_view src )
    {
        std::vector< std::string_view > out;
        for ( auto t : lex( src ) )
            out.push_back( t.text );
        return out;
    };

    using enum tok;
    using sv = std::string_view;

    assert(( kinds( "let x = 1 in x" ) ==
             std::vector{ keyword, identifier, op, integer,
                          keyword, identifier, end } ));

    /* Maximal munch: ‹<=› is a single token, ‹iffy› is an identifier
     * and ‹==›‹=› is lexed as ‹==› followed by ‹=›. */
    assert(( texts( "a<=b" ) == std::vector< sv >{ "a", "<=", "b", "" } ));
    assert(( kinds( "iffy if" ) == std::vector{ identifier, keyword, end } ));
    assert(( texts( "===" ) == std::vector< sv >{ "==", "=", "" } ));

    /* Comments and whitespace disappear entirely. */
    assert(( texts( "1 # one\n  + 2 # two" ) ==
             std::vector< sv >{ "1", "+", "2", "" } ));

    /* Strings keep their quotes in the lexeme. */
    assert(( texts( "\"hi there\"" ) ==
             std::vector< sv >{ "\"hi there\"", "" } ));

    /* Errors: a stray ‹!›, an unknown character and an unterminated
     * string. The lexer keeps going after each of them. */
    assert(( kinds( "a ! b" ) == std::vector{ identifier, error,
                                              identifier, end } ));
    assert(( kinds( "1 $ 2" ) == std::vector{ integer, error, integer, end } ));
    assert(( kinds( "\"abc\n1" ) == std::vector{ error, integer, end } ));

    /* Offsets point at the first character of each token. */
    auto toks = lex( "let  answer" );
    assert( toks[ 0 ].offset == 0 );
    assert( toks[ 1 ].offset == 5 );
    assert( toks[ 2 ].offset == 11 );

    return 0;
}
