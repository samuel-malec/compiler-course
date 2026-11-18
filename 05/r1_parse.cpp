#include <cassert>
#include <optional>
#include <string>
#include <string_view>

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
