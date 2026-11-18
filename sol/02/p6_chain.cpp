#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* In Python, ‹a < b < c› is not an error (as in ‹tiny›) and it does
 * not mean ‹(a < b) < c› (as in C): it means ‹a < b and b < c›, with
 * ‹b› evaluated only once. In this exercise, you will write an
 * evaluator for expressions with Python-style «chained comparisons»
 * and «short-circuiting» boolean operators. The grammar is:
 *
 *     expr  = conj, { 'or', conj } ;
 *     conj  = neg, { 'and', neg } ;
 *     neg   = 'not', neg | cmp ;
 *     cmp   = sum, { cmpop, sum } ;
 *     cmpop = '==' | '!=' | '<' | '<=' | '>' | '>=' ;
 *     sum   = prod, { ( '+' | '-' ), prod } ;
 *     prod  = atom, { ( '*' | '/' ), atom } ;
 *     atom  = number | ident | '(', expr, ')' ;
 *
 * where ‹number› is a sequence of digits and ‹ident› is a sequence of
 * lowercase letters other than ‹and›, ‹or› and ‹not›. Tokens may be
 * separated by spaces (and must be, where they would otherwise run
 * together, as in ‹a and b›).
 *
 * All values are of type ‹long›; the result of a comparison or of a
 * boolean operator is 1 (true) or 0 (false), and any non-zero value
 * counts as true. The semantics:
 *
 *  • ‹x₀ op₁ x₁ op₂ x₂ … opₙ xₙ› (with comparison operators ‹opᵢ›) is
 *    true iff all of ‹xᵢ₋₁ opᵢ xᵢ› are true; the operands are
 *    evaluated left to right, each at most once, and evaluation stops
 *    as soon as one of the comparisons is false,
 *  • ‹a and b› is 0 if ‹a› is false (and then ‹b› is «not
 *    evaluated»), otherwise it is 1 if ‹b› is true and 0 otherwise,
 *  • ‹a or b› is 1 if ‹a› is true (and then ‹b› is «not evaluated»),
 *    otherwise it is 1 if ‹b› is true and 0 otherwise,
 *  • ‹not a› is 1 if ‹a› is false and 0 otherwise,
 *  • the arithmetic operators work as in C++.
 *
 * Variables are looked up in ‹env›. Return ‹std::nullopt› if the
 * input is not syntactically valid (even in a part which is not
 * evaluated), or if the evaluation divides by zero or uses a variable
 * which is not in ‹env› («only» if that part is evaluated).
 *
 * The tricky part is to «parse» a sub-expression without
 * «evaluating» it. One simple way is to give the parser a flag that
 * says whether it should evaluate. */

// std::optional< long > eval( std::string_view expr,
//                             const std::map< std::string, long > &env );

/* Errors are signalled with an exception. The parser has a flag,
 * ‹live›, which says whether we are evaluating; when it is false,
 * the parser still checks syntax, but computes nothing (it returns 0
 * everywhere) and therefore cannot fail at run time. */

struct chain_error {};

struct chain_parser
{
    std::vector< std::string > toks;
    const std::map< std::string, long > &env;
    std::size_t pos = 0;
    bool live = true;

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    bool accept( std::string_view t )
    {
        if ( peek() != t )
            return false;
        ++ pos;
        return true;
    }

    /* Run ‹f› with evaluation switched off (if ‹skip› is true). */
    long maybe( bool skip, auto f )
    {
        bool saved = live;
        live = live && !skip;
        long v = f();
        live = saved;
        return v;
    }

    long expr()
    {
        long v = conj();
        while ( accept( "or" ) )
        {
            bool done = v != 0;
            long r = maybe( done, [ & ] { return conj(); } );
            v = done || r != 0;
        }
        return v;
    }

    long conj()
    {
        long v = neg();
        while ( accept( "and" ) )
        {
            bool done = v == 0;
            long r = maybe( done, [ & ] { return neg(); } );
            v = !done && r != 0;
        }
        return v;
    }

    long neg()
    {
        if ( accept( "not" ) )
            return neg() == 0;
        return cmp();
    }

    static bool compare( const std::string &op, long a, long b )
    {
        if ( op == "==" ) return a == b;
        if ( op == "!=" ) return a != b;
        if ( op == "<" )  return a < b;
        if ( op == "<=" ) return a <= b;
        if ( op == ">" )  return a > b;
        return a >= b;
    }

    long cmp()
    {
        long left = sum();
        bool any = false, result = true;

        while ( peek() == "==" || peek() == "!=" || peek() == "<" ||
                peek() == "<=" || peek() == ">" || peek() == ">=" )
        {
            std::string op = toks[ pos++ ];
            any = true;
            long right = maybe( !result, [ & ] { return sum(); } );
            result = result && compare( op, left, right );
            left = right;
        }

        return any ? result : left;
    }

    long sum()
    {
        long v = prod();
        while ( true )
            if      ( accept( "+" ) ) v += prod();
            else if ( accept( "-" ) ) v -= prod();
            else return v;
    }

    long prod()
    {
        long v = atom();
        while ( true )
            if ( accept( "*" ) )
                v *= atom();
            else if ( accept( "/" ) )
            {
                long r = atom();
                if ( live && r == 0 )
                    throw chain_error();
                v = live ? v / r : 0;
            }
            else return v;
    }

    long atom()
    {
        if ( accept( "(" ) )
        {
            long v = expr();
            if ( !accept( ")" ) )
                throw chain_error();
            return v;
        }

        std::string t = peek();
        if ( t.empty() || t == "and" || t == "or" || t == "not" )
            throw chain_error();
        ++ pos;

        if ( t[ 0 ] >= '0' && t[ 0 ] <= '9' )
            return std::stol( t );

        if ( t[ 0 ] >= 'a' && t[ 0 ] <= 'z' )
        {
            if ( !live )
                return 0;
            auto it = env.find( t );
            if ( it == env.end() )
                throw chain_error();
            return it->second;
        }

        throw chain_error();
    }
};

std::optional< long > eval( std::string_view in,
                            const std::map< std::string, long > &env )
{
    std::vector< std::string > toks;
    auto cls = []( char c )
    {
        if ( c >= 'a' && c <= 'z' ) return 1;
        if ( c >= '0' && c <= '9' ) return 2;
        return 0;
    };

    for ( std::size_t i = 0; i < in.size(); )
    {
        std::size_t start = i++;
        char c = in[ start ];

        if ( c == ' ' )
            continue;
        if ( cls( c ) )
            while ( i < in.size() && cls( in[ i ] ) == cls( c ) )
                ++ i;
        else if ( ( c == '=' || c == '!' || c == '<' || c == '>' ) &&
                  i < in.size() && in[ i ] == '=' )
            ++ i;
        else if ( std::string_view( "+-*/()<>" ).find( c ) == in.npos )
            return std::nullopt;

        toks.emplace_back( in.substr( start, i - start ) );
    }

    chain_parser p{ toks, env };

    try
    {
        long v = p.expr();
        if ( p.pos != toks.size() )
            return std::nullopt;
        return v;
    }
    catch ( chain_error ) { return std::nullopt; }
}

int main()
{
    std::map< std::string, long > env{ { "x", 1 }, { "y", 2 }, { "z", 3 } };
    auto ev = [ & ]( std::string_view e ) { return eval( e, env ); };

    assert( ev( "2 * 3 + 4" ) == 10 );
    assert( ev( "x + y * z" ) == 7 );
    assert( ev( "1 < 2" ) == 1 );
    assert( ev( "1 < 2 < 3" ) == 1 );
    assert( ev( "1 < 3 < 2" ) == 0 );
    assert( ev( "z > y > x" ) == 1 );
    assert( ev( "1 < 2 == 1" ) == 0 ); /* not ‹(1 < 2) == 1› */
    assert( ev( "x == 1 == x" ) == 1 );
    assert( ev( "(1 < 2) == 1" ) == 1 );
    assert( ev( "x + 1 == y" ) == 1 );
    assert( ev( "not 1 == 2" ) == 1 );
    assert( ev( "not not 5" ) == 1 );
    assert( ev( "5 and 7" ) == 1 );
    assert( ev( "0 or 0" ) == 0 );
    assert( ev( "1 == 1 and 2 == 3 or 4 > 3" ) == 1 );
    assert( ev( "x < y and y < z" ) == 1 );

    /* short-circuiting */
    assert( ev( "0 and 1 / 0" ) == 0 );
    assert( ev( "1 or 1 / 0" ) == 1 );
    assert( ev( "0 and w" ) == 0 );
    assert( ev( "2 > 3 < 1 / 0" ) == 0 );
    assert( ev( "0 and (1 or (2 / 0 == w))" ) == 0 );

    assert( !ev( "1 / 0 or 1" ) );
    assert( !ev( "1 < 2 < 1 / 0" ) );
    assert( !ev( "1 and w" ) );
    assert( !ev( "0 and (1 +" ) );
    assert( !ev( "0 and and" ) );
    assert( !ev( "1 2" ) );
    assert( !ev( "" ) );
    assert( !ev( "x <" ) );

    return 0;
}
