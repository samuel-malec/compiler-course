#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/* A tree-walking evaluator uses the C++ call stack to keep track of
 * which part of the tree it is working on. For deeply nested inputs,
 * this is a problem: the C++ stack is fairly small (typically 8 MiB),
 * and once it runs out, the program crashes. The fix is to keep the
 * stack «explicitly», in a data structure on the heap.
 *
 * Write a function ‹eval› which evaluates fully parenthesised prefix
 * expressions, like ‹(+ 1 (* 2 3))›. The expressions consist of
 * integer literals (non-empty sequences of digits) and binary
 * operations ‹(op a b)›, where ‹op› is one of ‹+›, ‹-› and ‹*›; tokens
 * are separated by spaces where needed (there may also be spaces
 * after ‹(› and before ‹)›). Return ‹std::nullopt› if the input is
 * malformed.
 *
 * The catch: the nesting depth of the input can be several hundred
 * thousand, so «neither the parser nor the evaluator may use
 * recursion». Instead, use a ‹std::vector› as a stack. One way is to
 * keep a stack of «pending operations»: each entry remembers the
 * operator and the operands evaluated so far. A number completes an
 * operand of the topmost pending operation; a ‹)› completes the
 * operation itself, which then becomes an operand of the next one.
 * You don't need to build a tree at all.
 *
 * You don't need to worry about overflow. */

// std::optional< long > eval( std::string_view expr );

/* A pending operation: the operator, and the operands we have so
 * far. The bottom of the stack is a pseudo-operation (‹op == 0›)
 * which collects the final result. */

struct pending
{
    char op;
    long args[ 2 ];
    int count;
};

std::optional< long > eval( std::string_view s )
{
    std::vector< pending > stack{ { 0, { 0, 0 }, 0 } };
    std::size_t i = 0;

    auto skip = [ & ] { while ( i < s.size() && s[ i ] == ' ' ) ++ i; };

    /* Deliver a finished value to the topmost pending operation. */
    auto deliver = [ & ]( long v )
    {
        auto &top = stack.back();
        int max = top.op ? 2 : 1;
        if ( top.count >= max )
            return false;
        top.args[ top.count++ ] = v;
        return true;
    };

    while ( skip(), i < s.size() )
    {
        char c = s[ i ];

        if ( c >= '0' && c <= '9' )
        {
            long v = 0;
            while ( i < s.size() && s[ i ] >= '0' && s[ i ] <= '9' )
                v = v * 10 + ( s[ i++ ] - '0' );
            if ( !deliver( v ) )
                return std::nullopt;
        }
        else if ( c == '(' )
        {
            ++ i;
            skip();
            if ( i >= s.size() || std::string_view( "+-*" ).find( s[ i ] ) == s.npos )
                return std::nullopt;
            /* the operator must be a token by itself */
            if ( i + 1 < s.size() && s[ i + 1 ] != ' ' && s[ i + 1 ] != '(' )
                return std::nullopt;
            if ( stack.back().count >= ( stack.back().op ? 2 : 1 ) )
                return std::nullopt;
            stack.push_back( { s[ i++ ], { 0, 0 }, 0 } );
        }
        else if ( c == ')' )
        {
            ++ i;
            auto top = stack.back();
            if ( !top.op || top.count != 2 )
                return std::nullopt;
            stack.pop_back();
            long a = top.args[ 0 ], b = top.args[ 1 ];
            long v = top.op == '+' ? a + b : top.op == '-' ? a - b : a * b;
            if ( !deliver( v ) )
                return std::nullopt;
        }
        else
            return std::nullopt;
    }

    if ( stack.size() != 1 || stack[ 0 ].count != 1 )
        return std::nullopt;
    return stack[ 0 ].args[ 0 ];
}

int main()
{
    assert( eval( "42" ) == 42 );
    assert( eval( "(+ 1 2)" ) == 3 );
    assert( eval( "(+ 1 (* 2 3))" ) == 7 );
    assert( eval( "( - (* 2 3) 10 )" ) == -4 );
    assert( eval( "(*(+ 1 1)(+ 2 2))" ) == 8 );

    assert( !eval( "" ) );
    assert( !eval( "(+ 1)" ) );
    assert( !eval( "(+ 1 2 3)" ) );
    assert( !eval( "(+ 1 2" ) );
    assert( !eval( "(+ 1 2))" ) );
    assert( !eval( "(/ 1 2)" ) );
    assert( !eval( "1 2" ) );
    assert( !eval( "()" ) );
    assert( !eval( "(1 2 3)" ) );

    const int depth = 300000;
    std::string right, left;

    for ( int i = 0; i < depth; ++ i )
        right += "(+ 1 ";
    right += "0" + std::string( depth, ')' );
    assert( eval( right ) == depth );

    for ( int i = 0; i < depth; ++ i )
        left += "(- ";
    left += "0";
    for ( int i = 0; i < depth; ++ i )
        left += " 1)";
    assert( eval( left ) == -depth ); /* ((0 - 1) - 1) - … */

    return 0;
}
