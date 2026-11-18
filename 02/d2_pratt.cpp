#include <cassert>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/* The parser in ‹d1_descent› has one method per precedence level,
 * and most of them look exactly the same. In this demo, we will
 * replace all of them with a single method, using a technique known
 * as «Pratt parsing» (after Vaughan Pratt, who described it in 1973)
 * or «precedence climbing».
 *
 * The idea is to assign each infix operator a pair of numbers, its
 * «left» and «right binding power». Think of an operand sitting
 * between two operators, like ‹b› in ‹a + b * c›: the operator with
 * the higher binding power on the side facing ‹b› ‘pulls’ it in.
 * Here, the left binding power of ‹*› is higher than the right
 * binding power of ‹+›, so ‹b› belongs to ‹*›.
 *
 * Associativity falls out of the asymmetry between the two numbers:
 * for a left-associative operator, the right binding power is
 * slightly «higher» than the left (so that in ‹a - b - c›, the first
 * ‹-› wins ‹b›), and for a right-associative operator, it is the
 * other way around.
 *
 * We will add two more operators which ‹d1_descent› does not have,
 * to show off the flexibility: the right-associative power operator
 * ‹^›, and a postfix factorial ‹!›. The table: */

struct binding { int left, right; };

const std::map< std::string, binding, std::less<> > infix =
{
    { "or",  { 1, 2 } },
    { "and", { 3, 4 } },
    { "==",  { 5, 6 } }, { "!=", { 5, 6 } },
    { "<",   { 5, 6 } }, { "<=", { 5, 6 } },
    { ">",   { 5, 6 } }, { ">=", { 5, 6 } },
    { "+",   { 7, 8 } }, { "-",  { 7, 8 } },
    { "*",   { 9, 10 } }, { "/", { 9, 10 } }, { "%", { 9, 10 } },
    { "^",   { 14, 13 } }, /* right-associative */
};

/* Prefix operators only have a right binding power, and postfix
 * operators only a left one. Since the binding power of unary minus
 * (11) is lower than that of ‹^›, ‹-2 ^ 2› means ‹-(2 ^ 2)›, as in
 * mathematics. The factorial binds tighter than anything else. */

const std::map< std::string, int, std::less<> > prefix =
{
    { "-", 11 }, { "not", 11 }
};

const std::map< std::string, int, std::less<> > postfix =
{
    { "!", 15 }
};

/* The lexer is a minor variation of the one in ‹d1_descent›. Since
 * ‹!› is now an operator by itself, it only forms a token with a
 * following ‹=› if the ‹=› is immediately there. */

bool is_digit( char c ) { return c >= '0' && c <= '9'; }
bool is_alpha( char c ) { return ( c >= 'a' && c <= 'z' ) || c == '_'; }

std::vector< std::string > lex( std::string_view s )
{
    std::vector< std::string > out;
    for ( std::size_t i = 0; i < s.size(); )
    {
        std::size_t start = i++;
        if ( s[ start ] == ' ' )
            continue;
        if ( is_alpha( s[ start ] ) || is_digit( s[ start ] ) )
            while ( i < s.size() && ( is_alpha( s[ i ] ) || is_digit( s[ i ] ) ) )
                ++ i;
        else if ( std::string_view( "=!<>" ).find( s[ start ] ) != s.npos &&
                  i < s.size() && s[ i ] == '=' )
            ++ i;
        out.emplace_back( s.substr( start, i - start ) );
    }
    out.emplace_back( "" );
    return out;
}

struct parse_error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

struct parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    const std::string &peek() const { return toks[ pos ]; }
    std::string next() { return toks[ pos++ ]; }

    void expect( std::string_view t )
    {
        if ( next() != t )
            throw parse_error( "expected " + std::string( t ) );
    }

    /* This is the whole parser for operators. It parses an
     * expression in which every operator «to the right» of the first
     * operand has left binding power at least ‹min_bp›. When it sees
     * an operator which binds less tightly, it stops and returns – the
     * operator will be handled by one of the callers up the stack.
     *
     * Let's trace ‹1 + 2 * 3 - 4› with ‹min_bp = 0›:
     *
     *  1. we read the operand ‹1›; the next operator is ‹+› with
     *     ‹left = 7 ≥ 0›, so we consume it and recurse with
     *     ‹min_bp = 8›,
     *  2. the recursive call reads ‹2›, then sees ‹*›, ‹9 ≥ 8›, and
     *     recurses with ‹min_bp = 10› – which reads ‹3› and stops at
     *     ‹-› (‹7 < 10›); back in step 2, we have ‹(* 2 3)›, and we
     *     stop at ‹-› as well (‹7 < 8›),
     *  3. back in step 1, we have ‹(+ 1 (* 2 3))›; ‹-› has ‹7 ≥ 0›,
     *     so we consume it and continue the loop – the result is
     *     ‹(- (+ 1 (* 2 3)) 4)›. */

    std::string expr( int min_bp = 0 )
    {
        std::string lhs = operand();
        bool after_cmp = false;

        while ( true )
        {
            const std::string &op = peek();

            if ( auto p = postfix.find( op ); p != postfix.end() )
            {
                if ( p->second < min_bp )
                    break;
                lhs = "(" + next() + " " + lhs + ")";
                continue;
            }

            auto i = infix.find( op );
            if ( i == infix.end() || i->second.left < min_bp )
                break;

            /* Comparisons are non-associative: two of them in a row on
             * the same level are an error. */
            bool is_cmp = i->second.left == 5;
            if ( is_cmp && after_cmp )
                throw parse_error( "comparisons do not associate" );
            after_cmp = is_cmp;

            next();
            std::string rhs = expr( i->second.right );
            lhs = "(" + op + " " + lhs + " " + rhs + ")";
        }

        return lhs;
    }

    /* An operand is an atom, a parenthesised expression, or a prefix
     * operator applied to an expression which binds at least as
     * tightly as the operator. Notice that ‹let› and ‹if› fit into
     * the same scheme: they are just prefix ‘operators’ with the
     * lowest possible binding power, so their body extends as far to
     * the right as possible. */

    std::string operand()
    {
        std::string t = next();

        if ( auto p = prefix.find( t ); p != prefix.end() )
            return "(" + t + " " + expr( p->second ) + ")";

        if ( t == "(" )
        {
            std::string e = expr();
            expect( ")" );
            return e;
        }

        if ( t == "let" )
        {
            std::string name = next();
            expect( "=" );
            std::string value = expr();
            expect( "in" );
            return "(let " + name + " " + value + " " + expr() + ")";
        }

        if ( t == "if" )
        {
            std::string c = expr();
            expect( "then" );
            std::string a = expr();
            expect( "else" );
            return "(if " + c + " " + a + " " + expr() + ")";
        }

        if ( !t.empty() && ( is_alpha( t[ 0 ] ) || is_digit( t[ 0 ] ) ) )
            return t;

        throw parse_error( "expected an operand" );
    }

    std::string parse()
    {
        std::string e = expr();
        if ( !peek().empty() )
            throw parse_error( "unexpected " + peek() );
        return e;
    }
};

std::string parse( std::string_view s ) { return parser{ lex( s ) }.parse(); }

bool fails( std::string_view s )
{
    try { parse( s ); return false; }
    catch ( const parse_error & ) { return true; }
}

int main() /* demo */
{
    assert( parse( "1 + 2 * 3 - 4" ) == "(- (+ 1 (* 2 3)) 4)" );
    assert( parse( "2 ^ 3 ^ 2" ) == "(^ 2 (^ 3 2))" );
    assert( parse( "-2 ^ 2" ) == "(- (^ 2 2))" );
    assert( parse( "2 ^ -1" ) == "(^ 2 (- 1))" );
    assert( parse( "-n!" ) == "(- (! n))" );
    assert( parse( "n! ^ 2" ) == "(^ (! n) 2)" );
    assert( parse( "a < b and not c or d" ) ==
            "(or (and (< a b) (not c)) d)" );
    assert( parse( "not a == b" ) == "(== (not a) b)" );
    assert( parse( "let x = 1 in x + 1" ) == "(let x 1 (+ x 1))" );
    assert( parse( "1 + let x = 1 in x * 2" ) == "(+ 1 (let x 1 (* x 2)))" );
    assert( parse( "(a < b) == (c < d)" ) == "(== (< a b) (< c d))" );

    assert( fails( "a < b < c" ) );
    assert( fails( "a < b == c" ) );
    assert( fails( "1 +" ) );
    assert( fails( "(1" ) );
    assert( fails( "1 2" ) );

    return 0;
}
