#include <cassert>
#include <optional>
#include <string>
#include <string_view>

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
