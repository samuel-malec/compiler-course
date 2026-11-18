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

/* A program in ML or Haskell is a list of top-level definitions,
 * which can refer to each other in any order (including recursively).
 * Treating all of them as one big ‹letrec› group would make them all
 * monomorphic within the program – a function could not be used at
 * two different types by the other definitions. Haskell's solution:
 * split the definitions into «strongly connected components» of the
 * dependency graph (definitions which depend on each other, directly
 * or indirectly), infer the components in dependency order, and
 * generalize each component before inferring the ones that use it.
 *
 * Write a function ‹infer_program› for programs of the form
 *
 *     (define NAME EXPR) …
 *
 * (read them by wrapping the text in parentheses). The expressions are
 * those of ‹p4_lists› (including the built-in lists and pairs, ‹let›
 * and ‹letrec›). A definition «depends on» another if it refers to its
 * name (as a free variable – a local binding with the same name
 * shadows it). The definitions of one component are inferred like a
 * ‹letrec› group: monomorphic within the group, then generalized
 * together. Unlike ‹letrec›, a definition does not need to be a ‹fn›.
 * A top-level name shadows a built-in of the same name.
 *
 * The result lists ‹NAME : TYPE› for each definition, in program order,
 * with each type normalized on its own. Return ‹std::nullopt› if any
 * definition has no type, or if a name is defined twice. */

// std::optional< std::vector< std::string > > infer_program( std::string_view program );

int main()
{
    using v = std::vector< std::string >;

    assert(( infer_program( "(define x 1)" ) == v{ "x : int" } ));

    /* ‹id› is used at two types by ‹both›, which comes «earlier» */
    assert(( infer_program( "(define both (fn () (pair (id 1) (id #t))))"
                            "(define id (fn (x) x))" ) ==
             v{ "both : (-> (pair int bool))", "id : (-> 'a 'a)" } ));

    assert(( infer_program( "(define even (fn (n) (if (= n 0) #t (odd (- n 1)))))"
                            "(define odd (fn (n) (if (= n 0) #f (even (- n 1)))))"
                            "(define test (pair (even 10) (odd 3)))" ) ==
             v{ "even : (-> int bool)", "odd : (-> int bool)", "test : (pair bool bool)" } ));

    assert(( infer_program( "(define lens (fn (l) (map length l)))"
                            "(define map (fn (f l) (if (null? l) nil (cons (f (head l)) (map f (tail l))))))"
                            "(define length (fn (l) (if (null? l) 0 (+ 1 (length (tail l))))))"
                            "(define names (map (fn (b) (if b 1 0)) (cons #t nil)))" ) ==
             v{ "lens : (-> (list (list 'a)) (list int))",
                "map : (-> (-> 'a 'b) (list 'a) (list 'b))",
                "length : (-> (list 'a) int)",
                "names : (list int)" } ));

    /* a local binding shadows the top-level ‹f›: no dependency */
    assert(( infer_program( "(define g (fn (f) (f 1)))"
                            "(define f (fn (x) (g (fn (y) x))))" ) ==
             v{ "g : (-> (-> int 'a) 'a)", "f : (-> 'a 'a)" } ));

    /* shadowing a built-in */
    assert(( infer_program( "(define head 5) (define x (+ head 1))" ) == v{ "head : int", "x : int" } ));

    assert( !infer_program( "(define x 1) (define x 2)" ) );
    assert( !infer_program( "(define f (fn (x) (f 1 2)))" ) );
    assert( !infer_program( "(define a b)" ) );
    assert( !infer_program( "(define p (pair (id 1) (id #t))) (define id (fn (x) (if (p) x x)))" ) );

    return 0;
}
