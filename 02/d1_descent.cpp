#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/* In this demo, we will write a recursive descent parser for the
 * expressions of ‹tiny›. To keep the focus on the parser, it will not
 * build a proper syntax tree yet (that is the topic of the next
 * chapter): instead, it will produce a fully parenthesised prefix
 * form of the input (an S-expression), which makes the structure
 * that the parser found explicit. For example, the input ‹1 + 2 * 3›
 * becomes ‹(+ 1 (* 2 3))›.
 *
 * The grammar, in EBNF:
 *
 *     expr  = 'let', ident, '=', expr, 'in', expr
 *           | 'if', expr, 'then', expr, 'else', expr
 *           | or ;
 *     or    = and, { 'or', and } ;
 *     and   = cmp, { 'and', cmp } ;
 *     cmp   = sum, [ cmpop, sum ] ;
 *     cmpop = '==' | '!=' | '<' | '<=' | '>' | '>=' ;
 *     sum   = prod, { ( '+' | '-' ), prod } ;
 *     prod  = unary, { ( '*' | '/' | '%' ), unary } ;
 *     unary = ( '-' | 'not' ), unary | atom ;
 *     atom  = integer | string | ident | 'true' | 'false'
 *           | '(', expr, ')' ;
 *
 * Notice a few things about this grammar:
 *
 *  • the precedence levels, from the loosest to the tightest, are:
 *    ‹let› and ‹if›, ‹or›, ‹and›, comparisons, ‹+ -›, ‹* / %›, and
 *    finally unary operators,
 *  • comparisons are «non-associative»: ‹cmp› allows at most one
 *    comparison operator, so ‹a < b < c› is a syntax error,
 *  • ‹let› and ‹if› extend «as far to the right as possible»: the
 *    body of ‹let› is an ‹expr›, so ‹let x = 1 in x + 1› means
 *    ‹let x = 1 in (x + 1)›; also, they can only appear at the top
 *    of an expression, or inside parentheses: ‹1 + if …› is a syntax
 *    error, while ‹1 + (if …)› is fine,
 *  • every alternative starts with a different token, so the parser
 *    can always decide what to do by looking at the next token.
 *
 * First, a minimal lexer. It produces lexemes as strings – the
 * parser can tell their kinds apart by looking at the first
 * character. The empty string marks the end of input. */

bool is_digit( char c ) { return c >= '0' && c <= '9'; }
bool is_alpha( char c )
{
    return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_';
}

std::vector< std::string > lex( std::string_view s )
{
    std::vector< std::string > out;
    std::size_t i = 0;

    while ( i < s.size() )
    {
        std::size_t start = i++;
        char c = s[ start ];

        if ( c == ' ' || c == '\n' )
            continue;
        if ( is_alpha( c ) )
            while ( i < s.size() && ( is_alpha( s[ i ] ) || is_digit( s[ i ] ) ) )
                ++ i;
        else if ( is_digit( c ) )
            while ( i < s.size() && is_digit( s[ i ] ) )
                ++ i;
        else if ( c == '"' )
        {
            while ( i < s.size() && s[ i ] != '"' )
                ++ i;
            ++ i; /* the closing quote */
        }
        else if ( std::string_view( "=!<>" ).find( c ) != std::string_view::npos &&
                  i < s.size() && s[ i ] == '=' )
            ++ i;

        out.emplace_back( s.substr( start, i - start ) );
    }

    out.emplace_back( "" );
    return out;
}

/* Errors are reported by throwing an exception. The message says
 * what the parser expected and what it found instead – this is
 * the minimum that a usable parser must do. */

struct parse_error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

bool is_keyword( const std::string &t )
{
    for ( auto kw : { "let", "in", "if", "then", "else", "and", "or",
                      "not", "true", "false" } )
        if ( t == kw )
            return true;
    return false;
}

/* The parser itself. There is exactly one method per nonterminal,
 * and the body of each method follows the corresponding rule of the
 * grammar. Compare them side by side. */

struct parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    const std::string &peek() const { return toks[ pos ]; }
    std::string next() { return toks[ pos++ ]; }

    /* Consume the token ‹t› if it is next, return whether it was. */
    bool accept( std::string_view t )
    {
        if ( peek() != t )
            return false;
        ++ pos;
        return true;
    }

    void expect( std::string_view t )
    {
        if ( !accept( t ) )
            fail( "'" + std::string( t ) + "'" );
    }

    [[noreturn]] void fail( const std::string &what )
    {
        std::string got = peek().empty() ? "end of input" : "'" + peek() + "'";
        throw parse_error( "expected " + what + ", got " + got );
    }

    static std::string sexpr( std::string op, std::string a, std::string b )
    {
        return "(" + op + " " + a + " " + b + ")";
    }

    /*     expr  = 'let', ident, '=', expr, 'in', expr
     *           | 'if', expr, 'then', expr, 'else', expr
     *           | or ;                                         */

    std::string expr()
    {
        if ( accept( "let" ) )
        {
            std::string name = ident();
            expect( "=" );
            std::string value = expr();
            expect( "in" );
            return "(let " + name + " " + value + " " + expr() + ")";
        }

        if ( accept( "if" ) )
        {
            std::string c = expr();
            expect( "then" );
            std::string t = expr();
            expect( "else" );
            return "(if " + c + " " + t + " " + expr() + ")";
        }

        return disj();
    }

    std::string ident()
    {
        if ( peek().empty() || !is_alpha( peek()[ 0 ] ) || is_keyword( peek() ) )
            fail( "identifier" );
        return next();
    }

    /* The rules ‹or›, ‹and›, ‹sum› and ‹prod› all have the same
     * shape: ‹x = y, { op, y } ;›. The loop builds the result from
     * the left, which makes all of these operators left-associative.
     * (The methods for ‹or› and ‹and› are called ‹disj› and ‹conj›,
     * since ‹or› and ‹and› are reserved words in C++.) */

    std::string disj()
    {
        std::string l = conj();
        while ( accept( "or" ) )
            l = sexpr( "or", l, conj() );
        return l;
    }

    std::string conj()
    {
        std::string l = cmp();
        while ( accept( "and" ) )
            l = sexpr( "and", l, cmp() );
        return l;
    }

    /*     cmp   = sum, [ cmpop, sum ] ;
     *
     * An «optional» part becomes an ‹if› (not a ‹while›). After one
     * comparison, we return – if another comparison operator follows,
     * whoever called us will not know what to do with it, and the
     * parse fails (‹a < b < c› is not allowed). */

    std::string cmp()
    {
        std::string l = sum();
        for ( auto op : { "==", "!=", "<", "<=", ">", ">=" } )
            if ( accept( op ) )
                return sexpr( op, l, sum() );
        return l;
    }

    std::string sum()
    {
        std::string l = prod();
        while ( peek() == "+" || peek() == "-" )
        {
            std::string op = next();
            l = sexpr( op, l, prod() );
        }
        return l;
    }

    std::string prod()
    {
        std::string l = unary();
        while ( peek() == "*" || peek() == "/" || peek() == "%" )
        {
            std::string op = next();
            l = sexpr( op, l, unary() );
        }
        return l;
    }

    /*     unary = ( '-' | 'not' ), unary | atom ;
     *
     * A prefix operator applies to another ‹unary›, so prefix
     * operators can be stacked: ‹- - x› or ‹not not x›. */

    std::string unary()
    {
        if ( peek() == "-" || peek() == "not" )
        {
            std::string op = next();
            return "(" + op + " " + unary() + ")";
        }
        return atom();
    }

    std::string atom()
    {
        const std::string &t = peek();

        if ( accept( "(" ) )
        {
            std::string inner = expr();
            expect( ")" );
            return inner;
        }

        if ( t == "true" || t == "false" || ( !t.empty() &&
             ( is_digit( t[ 0 ] ) || t[ 0 ] == '"' ||
               ( is_alpha( t[ 0 ] ) && !is_keyword( t ) ) ) ) )
            return next();

        fail( "an expression" );
    }

    /* The entry point: an expression followed by the end of input. */

    std::string parse()
    {
        std::string e = expr();
        if ( !peek().empty() )
            fail( "end of input" );
        return e;
    }
};

std::string parse( std::string_view src )
{
    return parser{ lex( src ) }.parse();
}

std::string error( std::string_view src )
{
    try { parse( src ); }
    catch ( const parse_error &e ) { return e.what(); }
    return "";
}

int main() /* demo */
{
    assert( parse( "1 + 2 * 3" ) == "(+ 1 (* 2 3))" );
    assert( parse( "(1 + 2) * 3" ) == "(* (+ 1 2) 3)" );
    assert( parse( "1 - 2 - 3" ) == "(- (- 1 2) 3)" );
    assert( parse( "- - x * y" ) == "(* (- (- x)) y)" );
    assert( parse( "a < b and not c or d" ) ==
            "(or (and (< a b) (not c)) d)" );
    assert( parse( "let x = 1 in x + 1" ) == "(let x 1 (+ x 1))" );
    assert( parse( "if a then b else let c = d in e" ) ==
            "(if a b (let c d e))" );
    assert( parse( "1 + (if a then 2 else 3)" ) == "(+ 1 (if a 2 3))" );
    assert( parse( "\"hi\" == x" ) == "(== \"hi\" x)" );

    assert( error( "1 +" ) == "expected an expression, got end of input" );
    assert( error( "a < b < c" ) == "expected end of input, got '<'" );
    assert( error( "1 + if a then b else c" ) ==
            "expected an expression, got 'if'" );
    assert( error( "let in = 3 in 4" ) == "expected identifier, got 'in'" );
    assert( error( "(1 + 2" ) == "expected ')', got end of input" );
    assert( error( "if a then b" ) == "expected 'else', got end of input" );

    return 0;
}
