#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <set>
#include <vector>

/* Write a parser which translates programs in a C-like syntax into the
 * S-expression form of the core language. The grammar:
 *
 *     program = { stmt } ;
 *     stmt    = 'var', ident, [ '=', expr ], ';'
 *             | ident, '=', expr, ';'
 *             | 'print', expr, ';'
 *             | 'if', '(', expr, ')', stmt, [ 'else', stmt ]
 *             | 'while', '(', expr, ')', stmt
 *             | 'break', ';' | 'continue', ';'
 *             | '{', { stmt }, '}' ;
 *     expr    = cmp, { ( '==' | '!=' ), cmp } ;
 *     cmp     = sum, { ( '<' | '<=' | '>' | '>=' ), sum } ;
 *     sum     = term, { ( '+' | '-' ), term } ;
 *     term    = unary, { ( '*' | '/' | '%' ), unary } ;
 *     unary   = '-', unary | integer | ident | '(', expr, ')' ;
 *
 * All binary operators are left-associative. Identifiers are
 * sequences of lowercase letters, digits and ‹_› starting with a
 * letter, other than the keywords ‹var›, ‹print›, ‹if›, ‹else›,
 * ‹while›, ‹break› and ‹continue›. Whitespace (spaces and newlines)
 * separates tokens. An ‹else› belongs to the nearest ‹if›.
 *
 * The output is the program written as an S-expression, with a single
 * space between elements:
 *
 *  • the whole program is a ‹(block …)›, and so is every ‹{ … }›,
 *  • statements map to ‹(var x e)›, ‹(var x)›, ‹(set x e)›, ‹(print e)›,
 *    ‹(if c s)›, ‹(if c s s)›, ‹(while c s)›, ‹(break)› and
 *    ‹(continue)›,
 *  • binary operators map to ‹(op a b)›, except that ‹==› becomes
 *    ‹=›; unary minus ‹-e› becomes ‹(- 0 e)›.
 *
 * Return ‹std::nullopt› if the input does not match the grammar. */

// std::optional< std::string > parse( std::string_view src );

struct syntax_error {};

struct stmt_parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    bool accept( std::string_view t ) { if ( peek() != t ) return false; ++ pos; return true; }
    void expect( std::string_view t ) { if ( !accept( t ) ) throw syntax_error(); }

    static bool is_ident( const std::string &t )
    {
        static const std::set< std::string > kw =
            { "var", "print", "if", "else", "while", "break", "continue" };
        return !t.empty() && t[ 0 ] >= 'a' && t[ 0 ] <= 'z' && !kw.contains( t );
    }

    std::string ident()
    {
        if ( !is_ident( peek() ) ) throw syntax_error();
        return toks[ pos++ ];
    }

    std::string stmt()
    {
        if ( accept( "var" ) )
        {
            std::string x = ident();
            std::string out = accept( "=" ) ? "(var " + x + " " + expr() + ")" : "(var " + x + ")";
            expect( ";" );
            return out;
        }
        if ( accept( "print" ) )
        {
            std::string e = expr();
            expect( ";" );
            return "(print " + e + ")";
        }
        if ( accept( "if" ) )
        {
            expect( "(" );
            std::string c = expr();
            expect( ")" );
            std::string t = stmt();
            if ( accept( "else" ) )
                return "(if " + c + " " + t + " " + stmt() + ")";
            return "(if " + c + " " + t + ")";
        }
        if ( accept( "while" ) )
        {
            expect( "(" );
            std::string c = expr();
            expect( ")" );
            return "(while " + c + " " + stmt() + ")";
        }
        if ( accept( "break" ) ) { expect( ";" ); return "(break)"; }
        if ( accept( "continue" ) ) { expect( ";" ); return "(continue)"; }
        if ( accept( "{" ) )
        {
            std::string out = "(block";
            while ( !accept( "}" ) )
            {
                if ( peek().empty() ) throw syntax_error();
                out += " " + stmt();
            }
            return out + ")";
        }

        std::string x = ident();
        expect( "=" );
        std::string e = expr();
        expect( ";" );
        return "(set " + x + " " + e + ")";
    }

    /* One level of left-associative binary operators. */
    std::string level( std::initializer_list< const char * > ops, std::string ( stmt_parser::*next )() )
    {
        std::string l = ( this->*next )();
        while ( true )
        {
            bool found = false;
            for ( auto op : ops )
                if ( accept( op ) )
                {
                    std::string name = std::string( op ) == "==" ? "=" : op;
                    l = "(" + name + " " + l + " " + ( this->*next )() + ")";
                    found = true;
                    break;
                }
            if ( !found )
                return l;
        }
    }

    std::string expr() { return level( { "==", "!=" }, &stmt_parser::cmp ); }
    std::string cmp()  { return level( { "<=", "<", ">=", ">" }, &stmt_parser::sum ); }
    std::string sum()  { return level( { "+", "-" }, &stmt_parser::term ); }
    std::string term() { return level( { "*", "/", "%" }, &stmt_parser::unary ); }

    std::string unary()
    {
        if ( accept( "-" ) )
            return "(- 0 " + unary() + ")";
        if ( accept( "(" ) )
        {
            std::string e = expr();
            expect( ")" );
            return e;
        }
        const std::string &t = peek();
        if ( !t.empty() && t[ 0 ] >= '0' && t[ 0 ] <= '9' )
            return toks[ pos++ ];
        return ident();
    }
};

std::optional< std::string > parse( std::string_view src )
{
    auto word = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_'; };
    std::vector< std::string > toks;

    for ( std::size_t i = 0; i < src.size(); )
    {
        std::size_t start = i++;
        char c = src[ start ];
        if ( c == ' ' || c == '\n' )
            continue;
        if ( word( c ) )
        {
            bool digit = c >= '0' && c <= '9';
            while ( i < src.size() && word( src[ i ] ) && ( !digit || ( src[ i ] >= '0' && src[ i ] <= '9' ) ) )
                ++ i;
        }
        else if ( ( c == '=' || c == '!' || c == '<' || c == '>' ) && i < src.size() && src[ i ] == '=' )
            ++ i;
        else if ( std::string_view( "=<>+-*/%(){};" ).find( c ) == src.npos )
            return std::nullopt;
        toks.emplace_back( src.substr( start, i - start ) );
    }

    stmt_parser p{ toks };
    try
    {
        std::string out = "(block";
        while ( p.pos < toks.size() )
            out += " " + p.stmt();
        return out + ")";
    }
    catch ( const syntax_error & ) { return std::nullopt; }
}

int main()
{
    assert( parse( "" ) == "(block)" );
    assert( parse( "print 1;" ) == "(block (print 1))" );
    assert( parse( "var x = 1 + 2 * 3; var y; y = x - -1;" ) ==
            "(block (var x (+ 1 (* 2 3))) (var y) (set y (- x (- 0 1))))" );
    assert( parse( "if (a == b) print 1; else { print 2; print 3; }" ) ==
            "(block (if (= a b) (print 1) (block (print 2) (print 3))))" );
    assert( parse( "if (a) if (b) print 1; else print 2;" ) ==
            "(block (if a (if b (print 1) (print 2))))" );
    assert( parse( "while (i < 10) { i = i + 1; if (i % 2 != 0) continue; break; }" ) ==
            "(block (while (< i 10) (block (set i (+ i 1)) (if (!= (% i 2) 0) (continue)) (break))))" );
    assert( parse( "x = (a - b) - c;\ny = a - (b - c);" ) ==
            "(block (set x (- (- a b) c)) (set y (- a (- b c))))" );
    assert( parse( "x = a < b == c >= d;" ) == "(block (set x (= (< a b) (>= c d))))" );
    assert( parse( "{ } { var v_2 = 0; }" ) == "(block (block) (block (var v_2 0)))" );

    assert( !parse( "print 1" ) );
    assert( !parse( "var = 1;" ) );
    assert( !parse( "var if = 1;" ) );
    assert( !parse( "if a print 1;" ) );
    assert( !parse( "{ print 1;" ) );
    assert( !parse( "x = ;" ) );
    assert( !parse( "else print 1;" ) );
    assert( !parse( "x = 1 $ 2;" ) );

    return 0;
}
