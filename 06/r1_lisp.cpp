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

/* Write an interpreter for a small dialect of Lisp (close to Scheme).
 * A program is a «sequence» of forms (you can read it by wrapping the
 * whole text in parentheses before passing it to ‹read›). Values are
 * integers, booleans, «symbols», the empty list, «pairs» (cons cells)
 * and procedures. The forms:
 *
 *  • ‹(define x e)› – at the top level only: bind (or re-bind) the
 *    global variable ‹x›; its value as a form is the «symbol» ‹x›,
 *  • integer literals, ‹#t›, ‹#f›, ‹nil› (the empty list), variables,
 *  • ‹(quote d)› – the datum ‹d› as a value: numbers are integers, other
 *    atoms are symbols (except ‹#t› and ‹#f›), and lists are lists,
 *  • ‹(lambda (x …) e₁ e₂ …)› – a procedure; the body is one or more
 *    expressions, evaluated in order (the value is that of the last),
 *  • ‹(if c t e)› – everything except ‹#f› counts as true,
 *  • ‹(let ((x e) …) b₁ b₂ …)› – all ‹e›s are evaluated in the outer
 *    environment, then the body as with ‹lambda›,
 *  • ‹(begin e₁ e₂ …)› – sequence,
 *  • a call ‹(f a …)›: ‹f› evaluates to a procedure, which is either a
 *    ‹lambda› or one of the «built-in» procedures, which are ordinary
 *    values bound to global variables:
 *    ◦ ‹+›, ‹-›, ‹*› and ‹<› of two integers, ‹=› of two integers,
 *    ◦ ‹cons›, ‹car›, ‹cdr›, ‹null?› (true for the empty list only),
 *      ‹pair?›, ‹not› (true for ‹#f› only), ‹list› (any number of
 *      arguments),
 *    ◦ ‹eq?› – true if both are the same integer, boolean or symbol,
 *      both are the empty list, or both are the same pair (identity).
 *
 * Global variables are looked up when they are used, so a ‹lambda›
 * may refer to a global which is only defined later (this is how
 * recursion and mutual recursion work). Local variables follow
 * lexical scoping, as usual.
 *
 * The result of ‹run› is the value of the last form of the program,
 * written as follows: integers in decimal, ‹#t› and ‹#f›, symbols as
 * their names, procedures as ‹<procedure>›, and lists as in Scheme:
 * ‹(1 2 3)›, ‹()›, and with a dotted tail if the list does not end with
 * the empty list: ‹(1 2 . 3)›. Return ‹std::nullopt› for malformed
 * programs, and for run-time errors (unbound variables, wrong types
 * or numbers of arguments, ‹car› of the empty list, …). */

// std::optional< std::string > run( std::string_view program );

int main()
{
    assert( run( "42" ) == "42" );
    assert( run( "(define x 5) (* x x)" ) == "25" );
    assert( run( "(define x 5)" ) == "x" );
    assert( run( "(quote (a (b 1) c))" ) == "(a (b 1) c)" );
    assert( run( "(cons 1 (cons 2 3))" ) == "(1 2 . 3)" );
    assert( run( "(list 1 #t (quote x) nil)" ) == "(1 #t x ())" );
    assert( run( "(car (cdr (list 1 2 3)))" ) == "2" );
    assert( run( "+" ) == "<procedure>" );
    assert( run( "(if nil 1 2)" ) == "1" );
    assert( run( "(let ((x 1) (y 2)) (let ((x y) (y x)) (list x y)))" ) == "(2 1)" );
    assert( run( "(eq? (quote a) (quote a))" ) == "#t" );
    assert( run( "(eq? (list 1) (list 1))" ) == "#f" );
    assert( run( "(let ((p (list 1))) (eq? p p))" ) == "#t" );

    auto prelude = "(define map (lambda (f l) (if (null? l) nil (cons (f (car l)) (map f (cdr l))))))"
                   "(define filter (lambda (p l) (if (null? l) nil"
                   "  (if (p (car l)) (cons (car l) (filter p (cdr l))) (filter p (cdr l))))))"
                   "(define foldl (lambda (f a l) (if (null? l) a (foldl f (f a (car l)) (cdr l)))))"
                   "(define range (lambda (a b) (if (< a b) (cons a (range (+ a 1) b)) nil)))"
                   "(define even? (lambda (n) (if (= n 0) #t (odd? (- n 1)))))"
                   "(define odd? (lambda (n) (if (= n 0) #f (even? (- n 1)))))";
    auto with = [ & ]( std::string body ) { return run( prelude + body ); };

    assert( with( "(map (lambda (x) (* x x)) (range 0 5))" ) == "(0 1 4 9 16)" );
    assert( with( "(filter even? (range 0 10))" ) == "(0 2 4 6 8)" );
    assert( with( "(foldl + 0 (range 1 101))" ) == "5050" );
    assert( with( "(foldl (lambda (acc x) (cons x acc)) nil (list 1 2 3))" ) == "(3 2 1)" );
    assert( with( "(map (lambda (f) (f 3)) (list (lambda (x) (+ x 1)) (lambda (x) (* x x))))" ) == "(4 9)" );
    assert( with( "(define adder (lambda (n) (lambda (x) (+ x n)))) (map (adder 10) (list 1 2))" ) == "(11 12)" );
    assert( run( "(define f (lambda () (g))) (define g (lambda () 7)) (f)" ) == "7" );
    assert( run( "(define x 1) (define f (lambda () x)) (define x 2) (f)" ) == "2" );
    assert( run( "(begin 1 2 3)" ) == "3" );

    assert( !run( "y" ) );
    assert( !run( "(car nil)" ) );
    assert( !run( "(+ 1 #t)" ) );
    assert( !run( "((lambda (x) x))" ) );
    assert( !run( "(1 2)" ) );
    assert( !run( "(lambda (x))" ) );
    assert( !run( "(let ((x 1)) (define y 2))" ) );
    assert( !run( "(quote a b)" ) );

    return 0;
}
