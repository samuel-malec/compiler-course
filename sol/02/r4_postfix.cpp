#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* Function calls, array indexing and member access (‹f(x)›, ‹a[i]›,
 * ‹obj.field›) are «postfix» operators: they follow their operand,
 * they bind tighter than any prefix or infix operator, and they can
 * be chained: ‹f(x)(y).a[1]›. Write a parser for the following
 * grammar:
 *
 *     expr    = sum ;
 *     sum     = product, { '+', product } ;
 *     product = unary, { '*', unary } ;
 *     unary   = '-', unary | postfix ;
 *     postfix = primary, { call | index | member } ;
 *     call    = '(', [ expr, { ',', expr } ], ')' ;
 *     index   = '[', expr, ']' ;
 *     member  = '.', ident ;
 *     primary = ident | number | '(', expr, ')' ;
 *
 * where ‹ident› is a non-empty sequence of lowercase letters and
 * ‹number› a non-empty sequence of digits. Spaces may appear between
 * tokens. The function ‹parse› should return the structure of the
 * expression as an S-expression:
 *
 *  • a call is printed as ‹(call F A₁ A₂ …)›, where ‹F› is the
 *    callee and ‹Aᵢ› the arguments,
 *  • indexing as ‹(index A I)›, member access as ‹(member A name)›,
 *  • binary operators as ‹(+ A B)› and ‹(* A B)›, negation as ‹(- A)›,
 *  • identifiers and numbers as themselves.
 *
 * Return ‹std::nullopt› if the input does not match the grammar. */

// std::optional< std::string > parse( std::string_view expr );

struct postfix_error {};

struct postfix_parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    bool accept( std::string_view t )
    {
        if ( peek() != t ) return false;
        ++ pos;
        return true;
    }

    void expect( std::string_view t ) { if ( !accept( t ) ) throw postfix_error(); }

    static bool is_ident( const std::string &t ) { return !t.empty() && t[ 0 ] >= 'a' && t[ 0 ] <= 'z'; }
    static bool is_number( const std::string &t ) { return !t.empty() && t[ 0 ] >= '0' && t[ 0 ] <= '9'; }

    std::string expr() { return sum(); }

    std::string sum()
    {
        std::string l = product();
        while ( accept( "+" ) )
            l = "(+ " + l + " " + product() + ")";
        return l;
    }

    std::string product()
    {
        std::string l = unary();
        while ( accept( "*" ) )
            l = "(* " + l + " " + unary() + ")";
        return l;
    }

    std::string unary()
    {
        if ( accept( "-" ) )
            return "(- " + unary() + ")";
        return postfix();
    }

    /* The postfix loop has the same shape as the loops for binary
     * operators: the result so far becomes the operand of the next
     * postfix operator, which gives left-to-right chaining. */

    std::string postfix()
    {
        std::string e = primary();

        while ( true )
            if ( accept( "(" ) )
            {
                std::string call = "(call " + e;
                if ( !accept( ")" ) )
                {
                    do
                        call += " " + expr();
                    while ( accept( "," ) );
                    expect( ")" );
                }
                e = call + ")";
            }
            else if ( accept( "[" ) )
            {
                e = "(index " + e + " " + expr() + ")";
                expect( "]" );
            }
            else if ( accept( "." ) )
            {
                if ( !is_ident( peek() ) )
                    throw postfix_error();
                e = "(member " + e + " " + toks[ pos++ ] + ")";
            }
            else
                return e;
    }

    std::string primary()
    {
        if ( accept( "(" ) )
        {
            std::string e = expr();
            expect( ")" );
            return e;
        }

        if ( is_ident( peek() ) || is_number( peek() ) )
            return toks[ pos++ ];

        throw postfix_error();
    }
};

std::optional< std::string > parse( std::string_view in )
{
    auto cls = []( char c ) { return c >= 'a' && c <= 'z' ? 1 : c >= '0' && c <= '9' ? 2 : 0; };
    std::vector< std::string > toks;

    for ( std::size_t i = 0; i < in.size(); )
    {
        std::size_t start = i++;
        char c = in[ start ];
        if ( c == ' ' )
            continue;
        if ( cls( c ) )
            while ( i < in.size() && cls( in[ i ] ) == cls( c ) ) ++ i;
        else if ( std::string_view( "+*-()[].," ).find( c ) == in.npos )
            return std::nullopt;
        toks.emplace_back( in.substr( start, i - start ) );
    }

    postfix_parser p{ toks };
    try
    {
        auto e = p.expr();
        if ( p.pos != toks.size() )
            return std::nullopt;
        return e;
    }
    catch ( postfix_error ) { return std::nullopt; }
}

int main()
{
    assert( parse( "x" ) == "x" );
    assert( parse( "f(x)" ) == "(call f x)" );
    assert( parse( "f()" ) == "(call f)" );
    assert( parse( "f(x, y, 1)" ) == "(call f x y 1)" );
    assert( parse( "f(x)(y)" ) == "(call (call f x) y)" );
    assert( parse( "a.b.c" ) == "(member (member a b) c)" );
    assert( parse( "a[i][j]" ) == "(index (index a i) j)" );
    assert( parse( "-a.b" ) == "(- (member a b))" );
    assert( parse( "(-a).b" ) == "(member (- a) b)" );
    assert( parse( "obj.method(1, 2 + 3)" ) ==
            "(call (member obj method) 1 (+ 2 3))" );
    assert( parse( "(f)(x)" ) == "(call f x)" );
    assert( parse( "(a + b).c" ) == "(member (+ a b) c)" );
    assert( parse( "a + b * c[0]" ) == "(+ a (* b (index c 0)))" );
    assert( parse( "f(g(x), y)[0].z * 2" ) ==
            "(* (member (index (call f (call g x) y) 0) z) 2)" );
    assert( parse( " f ( x ) . y " ) == "(member (call f x) y)" );

    assert( !parse( "" ) );
    assert( !parse( "a." ) );
    assert( !parse( "a.1" ) );
    assert( !parse( "a.(b)" ) );
    assert( !parse( "f(,)" ) );
    assert( !parse( "f(x,)" ) );
    assert( !parse( "f(x" ) );
    assert( !parse( "a[]" ) );
    assert( !parse( "a[1, 2]" ) );
    assert( !parse( "a b" ) );
    assert( !parse( ".a" ) );

    return 0;
}
