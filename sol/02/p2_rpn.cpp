#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* «Reverse Polish notation» (RPN, or «postfix» notation) writes each
 * operator after its operands: ‹a + b * c› becomes ‹a b c * +›.
 * It needs no parentheses and no precedence rules, and it can be
 * evaluated with a simple stack – which is why stack-based virtual
 * machines (see block 3) execute code that looks just like this.
 *
 * Write a function ‹to_rpn› which translates an infix expression to
 * RPN. The input consists of:
 *
 *  • operands: identifiers (sequences of lowercase letters) and
 *    numbers (sequences of digits),
 *  • the binary operators ‹+›, ‹-›, ‹*›, ‹/› and ‹^›,
 *  • parentheses,
 *  • spaces, which separate tokens and are otherwise ignored.
 *
 * There are 3 levels of precedence: ‹+ -› (lowest), ‹* /›, and ‹^›
 * (highest). The operator ‹^› is right-associative, the others are
 * left-associative. There are no unary operators. The output must
 * list the tokens in postfix order, separated by single spaces. If
 * the input is not a valid expression, return ‹std::nullopt›.
 *
 * You can either write a recursive descent parser (or a Pratt
 * parser) which emits the tokens as it goes, or look up Dijkstra's
 * «shunting-yard» algorithm, which does the job with an explicit
 * stack of operators. */

// std::optional< std::string > to_rpn( std::string_view infix );

/* A Pratt parser which emits tokens instead of building a tree: when
 * the parser finishes both operands of an operator, it emits the
 * operator – which is exactly postfix order. */

struct rpn_parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;
    std::string out = "";

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    void emit( const std::string &t )
    {
        out += out.empty() ? t : " " + t;
    }

    static int left_bp( const std::string &op )
    {
        if ( op == "+" || op == "-" ) return 1;
        if ( op == "*" || op == "/" ) return 3;
        if ( op == "^" ) return 6;
        return -1;
    }

    static int right_bp( const std::string &op )
    {
        return op == "^" ? 5 : left_bp( op ) + 1;
    }

    bool expr( int min_bp )
    {
        if ( !operand() )
            return false;

        while ( left_bp( peek() ) >= min_bp )
        {
            std::string op = toks[ pos++ ];
            if ( !expr( right_bp( op ) ) )
                return false;
            emit( op );
        }

        return true;
    }

    bool operand()
    {
        std::string t = peek();
        ++ pos;

        if ( t == "(" )
            return expr( 0 ) && peek() == ")" && ( ++ pos, true );

        if ( t.empty() || left_bp( t ) >= 0 || t == ")" )
            return false;

        emit( t );
        return true;
    }
};

std::optional< std::string > to_rpn( std::string_view in )
{
    auto letter = []( char c ) { return c >= 'a' && c <= 'z'; };
    auto digit = []( char c ) { return c >= '0' && c <= '9'; };

    std::vector< std::string > toks;
    for ( std::size_t i = 0; i < in.size(); )
    {
        std::size_t start = i++;
        char c = in[ start ];

        if ( c == ' ' )
            continue;
        if ( letter( c ) )
            while ( i < in.size() && letter( in[ i ] ) ) ++ i;
        else if ( digit( c ) )
            while ( i < in.size() && digit( in[ i ] ) ) ++ i;
        else if ( std::string_view( "+-*/^()" ).find( c ) == in.npos )
            return std::nullopt;

        /* Two adjacent operands without a space (like ‹a1›) would be
         * two tokens; that is an error anyway, the parser catches it. */
        toks.emplace_back( in.substr( start, i - start ) );
    }

    rpn_parser p{ toks };
    if ( !p.expr( 0 ) || p.pos != toks.size() )
        return std::nullopt;
    return p.out;
}

int main()
{
    assert( to_rpn( "a" ) == "a" );
    assert( to_rpn( "12 + 3" ) == "12 3 +" );
    assert( to_rpn( "a + b * c" ) == "a b c * +" );
    assert( to_rpn( "(a + b) * c" ) == "a b + c *" );
    assert( to_rpn( "a - b - c" ) == "a b - c -" );
    assert( to_rpn( "a - (b - c)" ) == "a b c - -" );
    assert( to_rpn( "a ^ b ^ c" ) == "a b c ^ ^" );
    assert( to_rpn( "(a ^ b) ^ c" ) == "a b ^ c ^" );
    assert( to_rpn( "a * b ^ c ^ d / e" ) == "a b c d ^ ^ * e /" );
    assert( to_rpn( "((x))" ) == "x" );
    assert( to_rpn( "a*b+c*d" ) == "a b * c d * +" );
    assert( to_rpn( "x / (y + z) ^ 2" ) == "x y z + 2 ^ /" );

    assert( !to_rpn( "" ) );
    assert( !to_rpn( "a +" ) );
    assert( !to_rpn( "+ a" ) );
    assert( !to_rpn( "(a" ) );
    assert( !to_rpn( "a)" ) );
    assert( !to_rpn( "a b" ) );
    assert( !to_rpn( "()" ) );
    assert( !to_rpn( "a + * b" ) );
    assert( !to_rpn( "a1" ) );

    return 0;
}
