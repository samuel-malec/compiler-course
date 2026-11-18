#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/* This first demo is a bird's eye view of the entire course. We will
 * write a complete (if very small) implementation of a language – a
 * calculator for integer arithmetic with ‹+›, ‹-›, ‹*› and
 * parentheses. Even a language this small is traditionally processed
 * in several separate «phases»:
 *
 *  1. «lexical analysis» (the «lexer») cuts the input text into
 *     «tokens» – the ‘words’ of the language,
 *  2. «syntactic analysis» (the «parser») arranges the tokens into a
 *     «tree», which captures the structure of the program,
 *  3. «evaluation» (the «interpreter») walks the tree and computes
 *     the result.
 *
 * A compiler replaces step 3 with «code generation»: instead of
 * computing the result, it emits instructions which compute the
 * result when they are executed later. We will get there in block 3.
 * Each of the phases gets a chapter (or several) of its own; here,
 * we only want to see how they fit together. Don't worry if some of
 * the details of the parser are not entirely clear yet – Chapter 2
 * explains them properly. */

/* Tokens are the output of the lexer. Each token has a «kind» (is it
 * a number? a plus sign?) and, for some kinds, a «value» (which
 * number?). Notice that we do not keep the text of the token at all:
 * once we know that a token is a ‹plus›, the character ‹+› is of no
 * further interest. */

enum class kind { number, plus, minus, star, lparen, rparen, end };

struct token
{
    kind k;
    int value = 0;
};

bool is_digit( char c ) { return c >= '0' && c <= '9'; }
bool is_space( char c ) { return c == ' ' || c == '\t' || c == '\n'; }

/* The lexer reads the input from left to right, skipping whitespace.
 * Notice that it has no idea about the structure of the expression:
 * it will happily accept ‹) ) + (›. Checking the structure is the job
 * of the parser; the lexer only rejects characters which cannot be
 * a part of any token. We also add an ‹end› token after the last
 * real one, so that the parser does not need to worry about running
 * off the end of the vector. */

std::vector< token > lex( std::string_view in )
{
    std::vector< token > out;
    std::size_t i = 0;

    while ( i < in.size() )
    {
        char c = in[ i ];

        if ( is_space( c ) )
        {
            ++ i;
            continue;
        }

        if ( is_digit( c ) )
        {
            /* A number is a maximal sequence of digits. We compute
             * its value while we are reading it – in other words,
             * the lexer «decodes» the lexeme. */
            int value = 0;
            while ( i < in.size() && is_digit( in[ i ] ) )
                value = value * 10 + ( in[ i++ ] - '0' );
            out.push_back( { kind::number, value } );
            continue;
        }

        switch ( c )
        {
            case '+': out.push_back( { kind::plus, 0 } ); break;
            case '-': out.push_back( { kind::minus, 0 } ); break;
            case '*': out.push_back( { kind::star, 0 } ); break;
            case '(': out.push_back( { kind::lparen, 0 } ); break;
            case ')': out.push_back( { kind::rparen, 0 } ); break;
            default:
                throw std::runtime_error( "unexpected character" );
        }

        ++ i;
    }

    out.push_back( { kind::end, 0 } );
    return out;
}

/* The output of the parser is a tree. Each «node» of the tree is
 * either a number (a leaf), or a binary operation with two
 * sub-trees. We use the token kinds to tell those apart, and
 * ‹std::unique_ptr› to make the type recursive. */

struct node;
using node_ptr = std::unique_ptr< node >;

struct node
{
    kind op;            /* ‹number›, ‹plus›, ‹minus› or ‹star› */
    int value = 0;      /* only used if ‹op› is ‹number› */
    node_ptr left, right;
};

/* The parser is driven by a «grammar», which describes how the
 * tokens can be combined. We write it in EBNF, where ‹{ x }› means
 * ‘zero or more repetitions of ‹x›’ and ‹|› means ‘or’:
 *
 *     expr   = term, { ( '+' | '-' ), term } ;
 *     term   = atom, { '*', atom } ;
 *     atom   = number | '(', expr, ')' ;
 *
 * Splitting the expression into ‹expr› and ‹term› is how the grammar
 * says that ‹*› binds tighter than ‹+›: an ‹expr› is a sum of
 * products, never a product of sums (unless there are parentheses).
 *
 * The parser is «recursive descent»: each rule of the grammar
 * becomes a method, and each method consumes the tokens that make up
 * the corresponding part of the input. */

struct parser
{
    std::vector< token > tokens;
    std::size_t pos = 0;

    const token &peek() const { return tokens[ pos ]; }
    token next() { return tokens[ pos++ ]; }

    void expect( kind k )
    {
        if ( next().k != k )
            throw std::runtime_error( "syntax error" );
    }

    node_ptr binary( kind op, node_ptr l, node_ptr r )
    {
        return std::make_unique< node >( op, 0, std::move( l ),
                                              std::move( r ) );
    }

    /* The loops in ‹expr› and ‹term› build the tree from left to
     * right: ‹1 - 2 - 3› becomes ‹(1 - 2) - 3›, which is what we
     * want for subtraction (the operator is «left-associative»). */

    node_ptr expr()
    {
        node_ptr left = term();

        while ( peek().k == kind::plus || peek().k == kind::minus )
        {
            kind op = next().k;
            left = binary( op, std::move( left ), term() );
        }

        return left;
    }

    node_ptr term()
    {
        node_ptr left = atom();

        while ( peek().k == kind::star )
        {
            next();
            left = binary( kind::star, std::move( left ), atom() );
        }

        return left;
    }

    node_ptr atom()
    {
        token t = next();

        if ( t.k == kind::number )
            return std::make_unique< node >( kind::number, t.value );

        if ( t.k == kind::lparen )
        {
            node_ptr inner = expr();
            expect( kind::rparen );
            return inner;
        }

        throw std::runtime_error( "syntax error" );
    }

    /* The whole input must be a single expression – anything left
     * over after it is an error. */

    node_ptr parse()
    {
        node_ptr root = expr();
        expect( kind::end );
        return root;
    }
};

/* Evaluation is now a simple recursive walk over the tree. Notice
 * that the evaluator does not need to care about precedence or
 * parentheses at all – those were already resolved by the parser and
 * are encoded in the «shape» of the tree. */

int eval( const node &n )
{
    switch ( n.op )
    {
        case kind::number: return n.value;
        case kind::plus:   return eval( *n.left ) + eval( *n.right );
        case kind::minus:  return eval( *n.left ) - eval( *n.right );
        case kind::star:   return eval( *n.left ) * eval( *n.right );
        default: throw std::logic_error( "bad node" );
    }
}

/* To make the shape of the tree visible, we can also print it, in a
 * fully parenthesised prefix form (an ‘S-expression’). */

std::string show( const node &n )
{
    auto bin = [ & ]( std::string op )
    {
        return "(" + op + " " + show( *n.left ) + " "
                              + show( *n.right ) + ")";
    };

    switch ( n.op )
    {
        case kind::number: return std::to_string( n.value );
        case kind::plus:   return bin( "+" );
        case kind::minus:  return bin( "-" );
        case kind::star:   return bin( "*" );
        default: throw std::logic_error( "bad node" );
    }
}

/* Finally, the three phases chained together. */

int calc( std::string_view text )
{
    parser p{ lex( text ) };
    return eval( *p.parse() );
}

std::string tree( std::string_view text )
{
    parser p{ lex( text ) };
    return show( *p.parse() );
}

int main() /* demo */
{
    assert( lex( "1 + 23" ).size() == 4 ); /* includes ‹end› */
    assert( lex( "1 + 23" )[ 2 ].value == 23 );
    assert( lex( ") ) + (" ).size() == 5 ); /* the lexer does not care */

    assert( calc( "7" ) == 7 );
    assert( calc( "1 + 2 * 3" ) == 7 );
    assert( calc( "(1 + 2) * 3" ) == 9 );
    assert( calc( "10 - 3 - 2" ) == 5 );

    assert( tree( "1 + 2 * 3" ) == "(+ 1 (* 2 3))" );
    assert( tree( "10 - 3 - 2" ) == "(- (- 10 3) 2)" );
    assert( tree( "2 * (3 - 1)" ) == "(* 2 (- 3 1))" );

    auto fails = []( std::string_view text )
    {
        try { calc( text ); return false; }
        catch ( const std::runtime_error & ) { return true; }
    };

    assert( fails( "1 + $" ) );   /* lexer error */
    assert( fails( "1 + " ) );    /* parser error: missing operand */
    assert( fails( "(1 + 2" ) );  /* parser error: missing ‹)› */
    assert( fails( "1 2" ) );     /* parser error: leftover input */

    return 0;
}
