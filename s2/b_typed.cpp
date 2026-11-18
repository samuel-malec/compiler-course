#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/* The programs in this exercise are written as S-expressions, so
 * that we can concentrate on evaluation instead of parsing. The
 * function ‹read› (provided) turns the text into a tree of ‹sexp›
 * nodes: each node is either an «atom» – a number, a name, a string
 * literal «including» its quotes (no escapes), or anything else
 * without spaces and parentheses – or a «list» of nodes. ‹offset›
 * is the position of the first character of the node in the input.
 * The function ‹write› does the opposite of ‹read›. If the input is
 * malformed, ‹read› throws ‹std::invalid_argument›. */

struct sexp
{
    std::string atom;           /* non-empty for atoms */
    std::vector< sexp > items;  /* elements of a list */
    std::size_t offset = 0;

    bool is_atom() const { return !atom.empty(); }
    bool is_list() const { return atom.empty(); }
};

void skip_space( std::string_view s, std::size_t &pos )
{
    while ( pos < s.size() && ( s[ pos ] == ' ' || s[ pos ] == '\n' ) )
        ++ pos;
}

sexp read_at( std::string_view s, std::size_t &pos )
{
    skip_space( s, pos );
    sexp out{ "", {}, pos };

    if ( pos >= s.size() || s[ pos ] == ')' )
        throw std::invalid_argument( "unexpected end of list or input" );

    if ( s[ pos ] == '(' )
    {
        for ( ++ pos; skip_space( s, pos ), pos >= s.size() || s[ pos ] != ')'; )
            out.items.push_back( read_at( s, pos ) );
        ++ pos;
        return out;
    }

    std::size_t start = pos;

    if ( s[ pos ] == '"' )
    {
        pos = s.find( '"', pos + 1 );
        if ( pos == s.npos )
            throw std::invalid_argument( "unterminated string" );
        ++ pos;
    }
    else
        while ( pos < s.size() && std::string_view( " \n()" ).find( s[ pos ] ) == s.npos )
            ++ pos;

    out.atom = s.substr( start, pos - start );
    return out;
}

sexp read( std::string_view s )
{
    std::size_t pos = 0;
    sexp e = read_at( s, pos );
    skip_space( s, pos );
    if ( pos != s.size() )
        throw std::invalid_argument( "trailing input" );
    return e;
}

std::string write( const sexp &e )
{
    if ( e.is_atom() )
        return e.atom;

    std::string out;
    for ( const auto &i : e.items )
        out += ( out.empty() ? "" : " " ) + write( i );
    return "(" + out + ")";
}

/* In this assignment, you will write the «semantic analysis» for a
 * small typed imperative language with structures and arrays – the
 * phase of a compiler which runs after parsing and finds all the
 * errors that the grammar cannot express. To keep the focus on
 * semantics, programs are written as S-expressions:
 *
 *     program = (program item …) ;
 *     item    = (struct NAME (FIELD type) …)
 *             | (fun NAME ((PARAM type) …) rtype stmt) ;
 *     type    = 'int' | 'bool' | NAME | (array type) ;
 *     rtype   = type | 'void' ;
 *
 *     stmt    = (block stmt …)
 *             | (var x expr) | (var x type expr)
 *             | (set lvalue expr)                 ; lvalue: x, (get …) or (at …)
 *             | (if expr stmt [stmt]) | (while expr stmt)
 *             | (return) | (return expr)
 *             | (do expr) | (print expr) ;
 *
 *     expr    = integer | #t | #f | x
 *             | (op expr expr) | (and expr expr) | (or expr expr) | (not expr)
 *             | (new S expr …)                    ; one value per field, in order
 *             | (get expr FIELD)                  ; field of a struct
 *             | (array type expr …)               ; an array literal
 *             | (at expr expr) | (len expr)       ; indexing, length
 *             | (f expr …) ;                      ; a call
 *
 * The operators are those of Chapter 7: ‹+ - * / %› on ‹int›s, the
 * comparisons ‹< <= > >=› on ‹int›s giving ‹bool›, and ‹=› and ‹!=› on two
 * values of the same type, which must be ‹int› or ‹bool›. Structures and
 * functions can refer to each other in any order; a structure may
 * contain fields of its own type (through arrays, since there is no
 * ‹null›). ‹(var x e)› declares ‹x› with the type of ‹e›; ‹(var x T e)›
 * requires ‹e : T›. Scopes are as in ‹05/p2_resolve›: blocks and the
 * bodies of ‹if› and ‹while› open new scopes, and the parameters are in
 * a scope around the body. A ‹void› function needs no ‹return›, and may
 * only use ‹(return)›; a non-void function must «always return» (as in
 * ‹07/p4_returns›), and only with ‹(return e)›. A call of a ‹void›
 * function may only appear directly in ‹(do …)›. ‹print› takes an ‹int›
 * or a ‹bool›.
 *
 * Write a function ‹check› which returns ‹std::nullopt› for a correct
 * program, and otherwise the «first» error, as ‹LINE:COL: MESSAGE›
 * (positions as in ‹07/p2_errors›). The checks are done in three
 * passes, each over the items in program order:
 *
 *  1. names: ‹duplicate struct S› (at the second ‹(struct›), ‹duplicate
 *     field S.f› (at the second field), ‹duplicate function f›,
 *  2. signatures: ‹unknown type X› (at the atom ‹X›) in field types and in
 *     function signatures (parameters in order, then the result type),
 *     and ‹duplicate variable x› for a repeated parameter (at the
 *     parameter),
 *  3. bodies, left to right, depth first:
 *     ◦ ‹unknown variable x› (at the atom), ‹unknown function f› (at the
 *       call), ‹unknown type X› (in ‹var› and ‹array›), ‹unknown struct S›
 *       (at ‹new›),
 *     ◦ ‹wrong number of arguments› (at a call or ‹new›),
 *     ◦ ‹expected T, got U› – at an expression of type ‹U› where ‹T› is
 *       required: operands, conditions, arguments, field values in
 *       ‹new›, array elements, indices, the value in ‹set› and in an
 *       annotated ‹var›, the value of ‹return› (in a ‹void› function, ‹T›
 *       is ‹void›); a ‹(return)› in a non-‹void› function is reported at
 *       the ‹return› as ‹expected T, got void›; ‹print› of a value of type
 *       ‹U› other than ‹int› and ‹bool› is ‹expected int, got U›,
 *     ◦ ‹cannot compare T› – at the first operand of ‹=› or ‹!=›,
 *     ◦ ‹not a struct: T› (at the operand of ‹get›), ‹no field f in S› (at
 *       the ‹get›), ‹not an array: T› (at the operand of ‹at› or ‹len›),
 *     ◦ ‹void value› – a call of a ‹void› function used as a value (at the
 *       call),
 *     ◦ ‹duplicate variable x› (at the ‹var›), ‹unreachable code› (at the
 *       statement), and finally ‹missing return in f› (at the ‹(fun›),
 *  4. any list which does not have one of the shapes above gives ‹bad
 *     form› (at the list), and an input which cannot be read gives
 *     ‹1:1: syntax error›.
 *
 * Types in messages are written with ‹write› (‹int›, ‹Point›, ‹(array
 * int)›, ‹void›). */

// std::optional< std::string > check( std::string_view program );

int main()
{
    auto ok = []( std::string_view p ) { return !check( p ); };
    auto wrap = []( std::string body ) { return "(program " + body + ")"; };

    std::string point = "(struct Point (x int) (y int))";
    std::string shapes =
        "(program"
        " (struct Tree (value int) (kids (array Tree)))"
        " (fun sum ((t Tree)) int (block (var s (get t value)) (var i 0)"
        "   (while (< i (len (get t kids))) (block (set s (+ s (sum (at (get t kids) i)))) (set i (+ i 1))))"
        "   (return s)))"
        " (fun leaf ((v int)) Tree (return (new Tree v (array Tree))))"
        " (fun main () void (block (var t (new Tree 1 (array Tree (leaf 2) (leaf 3))))"
        "   (set (at (get t kids) 0) (leaf 10)) (print (sum t)) (do (show t)) (return)))"
        " (fun show ((t Tree)) void (print (= (sum t) 0))))";
    assert( ok( shapes ) );

    assert( ok( wrap( point + " (fun f ((p Point)) bool (if (< (get p x) 0) (return #t) (return (= (get p y) 0))))" ) ) );
    assert( ok( wrap( "(fun f () void (block))" ) ) );

    assert( check( wrap( point + "\n" + point ) ) == "2:1: duplicate struct Point" );
    assert( check( wrap( "(struct S (a int) (a bool))" ) ) == "1:28: duplicate field S.a" );
    assert( check( wrap( "(fun f () void (do 1)) (fun f () void (do 2))" ) ) == "1:33: duplicate function f" );
    assert( check( wrap( "(struct S (a Q))" ) ) == "1:23: unknown type Q" );
    assert( check( wrap( "(fun f ((a int) (a int)) void (return))" ) ) == "1:26: duplicate variable a" );
    assert( check( wrap( "(fun f () int (return y))" ) ) == "1:32: unknown variable y" );
    assert( check( wrap( "(fun f () int (return (g)))" ) ) == "1:32: unknown function g" );
    assert( check( wrap( "(fun f () int (return (+ 1 #t)))" ) ) == "1:37: expected int, got bool" );
    assert( check( wrap( "(fun f () void (return 1))" ) ) == "1:33: expected void, got int" );
    assert( check( wrap( "(fun f () int (return))" ) ) == "1:24: expected int, got void" );
    assert( check( wrap( "(fun f () int (if #t (return 1)))" ) ) == "1:10: missing return in f" );
    assert( check( wrap( "(fun f () int (block (return 1) (do 2)))" ) ) == "1:42: unreachable code" );
    assert( check( wrap( point + " (fun f ((p Point)) int (return (get p z)))" ) ) == "1:72: no field z in Point" );
    assert( check( wrap( "(fun f ((a int)) int (return (get a x)))" ) ) == "1:44: not a struct: int" );
    assert( check( wrap( "(fun f ((a int)) int (return (len a)))" ) ) == "1:44: not an array: int" );
    assert( check( wrap( point + " (fun f ((p Point)) bool (return (= p p)))" ) ) == "1:76: cannot compare Point" );
    assert( check( wrap( "(fun g () void (return)) (fun f () int (return (g)))" ) ) == "1:57: void value" );
    assert( check( wrap( "(fun f () void (block (var a 1) (var a 2)))" ) ) == "1:42: duplicate variable a" );
    assert( check( wrap( point + " (fun f () Point (return (new Point 1)))" ) ) == "1:65: wrong number of arguments" );
    assert( check( wrap( "(fun f () void (print (array int)))" ) ) == "1:32: expected int, got (array int)" );
    assert( check( wrap( "(fun f () void (do (array int 1 #f)))" ) ) == "1:42: expected int, got bool" );
    assert( check( wrap( "(fun f () void (set 1 2))" ) ) == "1:25: bad form" );
    assert( check( "(program" ) == "1:1: syntax error" );

    return 0;
}
