#include <cassert>
#include <string>
#include <string_view>

/* In this assignment, you will implement a complete interpreter for a
 * small dynamically typed scripting language with C-like syntax,
 * first-class functions and arrays – roughly the kind of language
 * that JavaScript or Lox are. It brings together everything from
 * blocks 1 and 2: a lexer, a parser for statements and expressions,
 * scopes, closures, recursion and non-local control flow.
 *
 * ── Syntax ─────────────────────────────────────────────────────────
 *
 *     program = { decl } ;
 *     decl    = 'fun', ident, '(', [ params ], ')', block
 *             | stmt ;
 *     params  = ident, { ',', ident } ;
 *     stmt    = 'var', ident, '=', expr, ';'
 *             | 'if', '(', expr, ')', stmt, [ 'else', stmt ]
 *             | 'while', '(', expr, ')', stmt
 *             | 'return', [ expr ], ';'
 *             | 'break', ';' | 'continue', ';'
 *             | 'print', expr, ';'
 *             | block
 *             | expr, [ '=', expr ], ';' ;
 *     block   = '{', { decl }, '}' ;
 *
 *     expr    = and, { '||', and } ;
 *     and     = eq, { '&&', eq } ;
 *     eq      = cmp, [ ( '==' | '!=' ), cmp ] ;
 *     cmp     = sum, [ ( '<' | '<=' | '>' | '>=' ), sum ] ;
 *     sum     = term, { ( '+' | '-' ), term } ;
 *     term    = unary, { ( '*' | '/' | '%' ), unary } ;
 *     unary   = ( '-' | '!' ), unary | postfix ;
 *     postfix = primary, { '(', [ args ], ')' | '[', expr, ']' } ;
 *     args    = expr, { ',', expr } ;
 *     primary = integer | string | 'true' | 'false' | 'nil' | ident
 *             | '(', expr, ')'
 *             | '[', [ args ], ']'
 *             | 'fun', '(', [ params ], ')', block ;
 *
 * Identifiers are letters, digits and ‹_›, starting with a letter or
 * ‹_›, other than the keywords (‹fun›, ‹var›, ‹if›, ‹else›, ‹while›,
 * ‹return›, ‹break›, ‹continue›, ‹print›, ‹true›, ‹false›, ‹nil›).
 * Integers are sequences of digits. Strings are enclosed in ‹"›, with
 * the escapes ‹\n›, ‹\"› and ‹\\›. Comments start with ‹//› and extend to
 * the end of the line. In an assignment ‹e₁ = e₂;›, ‹e₁› must be a
 * variable or an indexing expression ‹a[i]› (this is checked by the
 * parser). ‹==› and ‹!=› and the comparisons are non-associative.
 *
 * ── Semantics ──────────────────────────────────────────────────────
 *
 * Values are ‹nil›, integers (‹long›), booleans, strings, arrays and
 * functions. Arrays and functions are «references»: assigning an
 * array to another variable does not copy it, and changes through one
 * variable are visible through the other.
 *
 *  • Scoping is lexical; a block opens a new scope; ‹var› declares a
 *    variable in the current scope (it is an error to declare the same
 *    name twice in one scope); ‹fun name(…) {…}› declares ‹name› in the
 *    current scope «before» the function is created, so it can call
 *    itself. Functions are closures: they capture the scope in which
 *    they were created (by reference – they see later assignments).
 *    Assignment changes the nearest variable of that name.
 *  • ‹+› adds two integers or concatenates two strings; ‹- * / %› work on
 *    integers (‹/› and ‹%› truncate, as in C++); unary ‹-› negates an
 *    integer.
 *  • ‹==› and ‹!=› work on any two values: two values are equal if they
 *    are both ‹nil›, or are integers, booleans or strings with the same
 *    value, or are the «same» array or function (identity). The
 *    comparisons ‹< <= > >=› work on two integers or two strings.
 *  • ‹false› and ‹nil› are «falsy», everything else is «truthy» (also ‹0›
 *    and ‹""›). ‹!›, ‹&&› and ‹||› work on truthiness and give a boolean;
 *    ‹&&› and ‹||› do not evaluate the right operand if not needed. The
 *    conditions of ‹if› and ‹while› use truthiness, too.
 *  • ‹a[i]› indexes an array (or a string, giving a one-character
 *    string), with ‹i› an integer from ‹0› to ‹len - 1›.
 *  • A call evaluates the callee, then the arguments from left to
 *    right, and runs the body with the parameters bound in a new scope
 *    (whose parent is the captured scope). ‹return› ends the call; a
 *    function which ends without ‹return› (or with a bare ‹return;›)
 *    returns ‹nil›. ‹break› and ‹continue› refer to the innermost
 *    enclosing ‹while› in the same function; ‹return› outside of a
 *    function, and ‹break› or ‹continue› outside of a loop, are syntax
 *    errors (check them when parsing).
 *  • Built-in functions (ordinary values, bound in the global scope,
 *    which a program may shadow): ‹len(x)› – the length of a string or
 *    an array; ‹push(a, v)› – append ‹v› to the array ‹a›, returns ‹nil›;
 *    ‹str(v)› – the string which ‹print› would print for ‹v›.
 *  • ‹print e;› writes the value followed by a newline: integers in
 *    decimal, ‹true› / ‹false›, ‹nil›, strings as they are, functions as
 *    ‹<fun>›, arrays as ‹[e₁, e₂, …]› where the elements are written the
 *    same way, except that strings «inside» arrays are written in double
 *    quotes (without escaping anything).
 *
 * ── Interface ──────────────────────────────────────────────────────
 *
 * Write a function ‹run( program )›, which returns the output of the
 * program. If the program is syntactically invalid, the result is
 * just ‹syntax error\n› (nothing is executed). If a run-time error
 * happens, the output so far is followed by ‹runtime error: MSG\n›,
 * where ‹MSG› is one of: ‹undefined variable X› (reading or assigning
 * a variable which does not exist), ‹redefined variable X›, ‹type
 * error› (an operation on values of the wrong types, including calling
 * a built-in with arguments of wrong types), ‹division by zero›,
 * ‹index out of range›, ‹not a function›, ‹wrong number of arguments›.
 *
 * Deep recursion does not need to be supported beyond a few thousand
 * levels, and memory may be freed only at the end of ‹run› (see the
 * note on arenas in ‹06/00_intro.txt›). */

// std::string run( std::string_view program );

int main()
{
    assert( run( "print 1 + 2 * 3;" ) == "7\n" );
    assert( run( "print \"a\" + \"b\"; print true; print nil; print [1, \"x\", [true]];" ) ==
            "ab\ntrue\nnil\n[1, \"x\", [true]]\n" );
    assert( run( "var x = 10; { var x = 20; print x; x = 30; } print x;" ) == "20\n10\n" );

    assert( run( "fun fact(n) { if (n <= 1) return 1; return n * fact(n - 1); }\n"
                 "print fact(10);" ) == "3628800\n" );

    assert( run( "fun counter() { var n = 0; return fun() { n = n + 1; return n; }; }\n"
                 "var a = counter(); var b = counter();\n"
                 "a(); a(); print a(); print b();" ) == "3\n1\n" );

    assert( run( "var xs = [];\n"
                 "var i = 0;\n"
                 "while (true) { i = i + 1; if (i % 2 == 0) continue; if (i > 7) break; push(xs, i); }\n"
                 "print xs; print len(xs);" ) == "[1, 3, 5, 7]\n4\n" );

    assert( run( "fun map(f, a) { var r = []; var i = 0;\n"
                 "  while (i < len(a)) { push(r, f(a[i])); i = i + 1; } return r; }\n"
                 "print map(fun(x) { return x * x; }, [1, 2, 3]);" ) == "[1, 4, 9]\n" );

    assert( run( "var a = [1, 2]; var b = a; b[0] = 5; print a; print a == b; print [1] == [1];" ) ==
            "[5, 2]\ntrue\nfalse\n" );
    assert( run( "print nil && x; print 0 && nil; print nil || \"y\"; print !0;" ) == "false\nfalse\ntrue\nfalse\n" );
    assert( run( "var s = \"hello\"; print s[1] + str(len(s)); // comment\n" ) == "e5\n" );
    assert( run( "fun f() { } print f(); print f; print str([\"q\"]);" ) == "nil\n<fun>\n[\"q\"]\n" );

    assert( run( "print 1; print x;" ) == "1\nruntime error: undefined variable x\n" );
    assert( run( "print 1 + true;" ) == "runtime error: type error\n" );
    assert( run( "print [1][1];" ) == "runtime error: index out of range\n" );
    assert( run( "print 7 / (2 - 2);" ) == "runtime error: division by zero\n" );
    assert( run( "var f = 5; f();" ) == "runtime error: not a function\n" );
    assert( run( "fun f(a) { } f(1, 2);" ) == "runtime error: wrong number of arguments\n" );
    assert( run( "var a = 1; var a = 2;" ) == "runtime error: redefined variable a\n" );

    assert( run( "print 1" ) == "syntax error\n" );
    assert( run( "print 1; 1 + 2 = 3;" ) == "syntax error\n" );
    assert( run( "break;" ) == "syntax error\n" );
    assert( run( "fun f() { while (true) { fun g() { break; } } }" ) == "syntax error\n" );
    assert( run( "return 1;" ) == "syntax error\n" );
    assert( run( "print 1 < 2 < 3;" ) == "syntax error\n" );

    return 0;
}
