#include <cassert>
#include <expected>
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

/* The core of ML and Haskell, as far as types go, is Hindley–Milner
 * inference «plus» algebraic data types and pattern matching. In this
 * assignment, you will implement both, together with the check which
 * makes pattern matching safe: «exhaustiveness» (every possible value
 * is matched by some case) and «redundancy» (every case can match
 * something that the previous cases do not).
 *
 * A program is a list of data type declarations and definitions:
 *
 *     program = (program item …) ;
 *     item    = (data HEAD (CON type …) …)
 *             | (define name expr) ;
 *     HEAD    = NAME | (NAME 'a …) ;
 *
 * A data declaration introduces a type constructor ‹NAME› with zero or
 * more distinct parameters (type variables), and one or more «value
 * constructors», each with a list of field types:
 *
 *     (data (list 'a) (Nil) (Cons 'a (list 'a)))
 *     (data color (Red) (Green) (Blue))
 *
 * In types, a data type with no parameters is written as an atom
 * (‹color›), one with parameters as a list (‹(list int)›); besides those,
 * types are ‹int›, ‹bool›, ‹(-> T … R)› and type variables (only the
 * parameters of the declaration may appear in it). Data types can be
 * recursive, and can refer to each other in any order. Constructor
 * names start with an uppercase letter, variables with a lowercase one.
 *
 * Expressions are those of ‹08/p3_letrec› (without the built-ins of
 * ‹p4_lists›), plus:
 *
 *  • a constructor: a bare ‹CON› if it has no fields, ‹(CON e …)› with
 *    exactly as many arguments as fields otherwise (constructors are
 *    not first-class values),
 *  • ‹(match e (pattern body) …)› with at least one case; a pattern is
 *    ‹_›, a variable, an integer literal, ‹#t›, ‹#f›, a bare ‹CON› without
 *    fields, or ‹(CON pattern …)› with one pattern per field; the
 *    variables of one pattern must be distinct; they are bound
 *    (monomorphically) in the body.
 *
 * Constructors have the obvious polymorphic types (‹Cons : ∀a. 'a →
 * (list 'a) → (list 'a)›), all the patterns must have the type of the
 * matched expression, and all the bodies the same type, which is the
 * type of the ‹match›. A ‹(define name e)› is inferred like a ‹letrec›
 * with a single binding (‹e› may refer to ‹name›), but ‹e› need not be a
 * ‹fn›; its type is generalized, and it is visible in the following
 * definitions (not in the preceding ones).
 *
 * After the type of a definition has been inferred, check every
 * ‹match› in it (in pre-order): first, each case in turn must not be
 * «redundant» – there must be a value matched by it but by none of the
 * previous cases; then the whole match must be «exhaustive». A value of
 * a data type can be any of its constructors (with any field values);
 * a ‹bool› can be ‹#t› or ‹#f›; an ‹int› any integer, so only a variable
 * or ‹_› covers all of them. The classic algorithm for both checks is
 * Maranget's «usefulness» of a pattern vector with respect to a
 * pattern matrix (look up ‘Warnings for pattern matching’).
 *
 * Write a function ‹infer› which returns, for a correct program, the
 * list ‹name : TYPE› for the definitions, in order, each type in normal
 * form (as in Chapter 8). Otherwise return the «first» error:
 *
 *  • ‹syntax error› – the input cannot be read, or an item is not of one
 *    of the two forms,
 *  • ‹bad data NAME› – the first (in program order) data declaration
 *    which is malformed, declares a type name or a constructor name
 *    which was already declared, has repeated parameters, or uses an
 *    unknown type, a type with a wrong number of arguments or a type
 *    variable which is not a parameter (all data declarations are
 *    checked before any definition),
 *  • ‹duplicate define NAME›,
 *  • ‹type error in NAME› – the definition does not have a type, or it
 *    is malformed in any way (unbound variable, unknown constructor,
 *    wrong number of fields, repeated pattern variable, …),
 *  • ‹redundant pattern in NAME›, ‹non-exhaustive match in NAME›. */

// std::expected< std::vector< std::string >, std::string > infer( std::string_view program );

int main()
{
    using v = std::vector< std::string >;
    auto err = []( std::string_view p ) { return infer( p ).error(); };

    std::string list = "(data (list 'a) (Nil) (Cons 'a (list 'a)))";
    std::string option = "(data (option 'a) (None) (Some 'a))";
    std::string tree = "(data (tree 'a) (Leaf) (Node (tree 'a) 'a (tree 'a)))";
    auto prog = [ & ]( std::string body ) { return "(program " + list + " " + option + " " + tree + " " + body + ")"; };

    assert(( infer( prog( "(define xs (Cons 1 (Cons 2 Nil)))" ) ) == v{ "xs : (list int)" } ));
    assert(( infer( prog( "(define empty Nil) (define two (Cons empty (Cons (Cons #t Nil) Nil)))" ) ) ==
             v{ "empty : (list 'a)", "two : (list (list bool))" } ));

    assert(( infer( prog( "(define map (fn (f l) (match l (Nil Nil) ((Cons x rest) (Cons (f x) (map f rest))))))"
                          "(define len (fn (l) (match l (Nil 0) ((Cons _ t) (+ 1 (len t))))))"
                          "(define test (len (map (fn (n) (< n 2)) (Cons 1 Nil))))" ) ) ==
             v{ "map : (-> (-> 'a 'b) (list 'a) (list 'b))", "len : (-> (list 'a) int)", "test : int" } ));

    assert(( infer( prog( "(define head (fn (l) (match l (Nil None) ((Cons x _) (Some x)))))" ) ) ==
             v{ "head : (-> (list 'a) (option 'a))" } ));

    assert(( infer( prog( "(define depth (fn (t) (match t (Leaf 0)"
                          "  ((Node l _ r) (let a (depth l) (let b (depth r) (+ 1 (if (> a b) a b))))))))" ) ) ==
             v{ "depth : (-> (tree 'a) int)" } ));

    assert(( infer( "(program (data color (Red) (Green) (Blue))"
                    " (define warm (fn (c) (match c (Red #t) (_ #f))))"
                    " (define name (fn (c) (match c (Red 1) (Green 2) (Blue 3)))))" ) ==
             v{ "warm : (-> color bool)", "name : (-> color int)" } ));

    /* nested patterns, booleans and integers */
    std::string zip = "(define zip (fn (a b) (match (Pair a b)"
                      "  ((Pair Nil _) Nil) ((Pair _ Nil) Nil)"
                      "  ((Pair (Cons x xs) (Cons y ys)) (Cons (Pair x y) (zip xs ys))))))";
    std::string pair = "(data (pair 'a 'b) (Pair 'a 'b))";
    assert(( infer( "(program " + list + " " + pair + " " + zip + ")" ) ==
             v{ "zip : (-> (list 'a) (list 'b) (list (pair 'a 'b)))" } ));
    assert(( infer( "(program (define f (fn (b n) (match b (#t (match n (0 1) (k k))) (#f 0)))))" ) ==
             v{ "f : (-> bool int int)" } ));

    /* mutually recursive data types, declared in any order */
    assert(( infer( "(program (data forest (Empty) (Trees tree forest)) (data tree (Tree int forest))"
                    " (define t (Tree 1 (Trees (Tree 2 Empty) Empty))))" ) == v{ "t : tree" } ));

    assert( err( "(program (data (list 'a) (Nil) (Cons 'a (list 'a))) (define f (fn (l) (match l (Nil 0)))))" ) ==
            "non-exhaustive match in f" );
    assert( err( prog( "(define f (fn (l) (match l (_ 0) (Nil 1))))" ) ) == "redundant pattern in f" );
    assert( err( prog( "(define f (fn (n) (match n (0 1) (1 2))))" ) ) == "non-exhaustive match in f" );
    assert( err( "(program " + list + " " + pair +
                 " (define f (fn (p) (match p ((Pair Nil _) 0) ((Pair (Cons x _) Nil) 1)))))" ) ==
            "non-exhaustive match in f" );
    assert( err( "(program " + list + " " + pair +
                 " (define f (fn (p) (match p ((Pair Nil _) 0) ((Pair _ Nil) 1) ((Pair (Cons _ _) _) 2) ((Pair _ (Cons _ _)) 3)))))" ) ==
            "redundant pattern in f" );
    assert( err( prog( "(define f (fn (b) (match b (#t 1) (#t 2) (#f 3))))" ) ) == "redundant pattern in f" );

    assert( err( prog( "(define f (Cons 1 (Cons #t Nil)))" ) ) == "type error in f" );
    assert( err( prog( "(define f (match 1 (Nil 0) (_ 1)))" ) ) == "type error in f" );
    assert( err( prog( "(define f (fn (p) (match p ((Cons x x) x) (Nil 0))))" ) ) == "type error in f" );
    assert( err( prog( "(define f (Cons 1))" ) ) == "type error in f" );
    assert( err( prog( "(define f (Foo 1))" ) ) == "type error in f" );
    assert( err( prog( "(define f g) (define g 1)" ) ) == "type error in f" );
    assert( err( prog( "(define f 1) (define f 2)" ) ) == "duplicate define f" );
    assert( err( "(program (data (list 'a) (Nil) (Cons 'b (list 'a))))" ) == "bad data list" );
    assert( err( "(program (data t (A int)) (data u (A bool)))" ) == "bad data u" );
    assert( err( "(program (data (box 'a) (Box (box))))" ) == "bad data box" );
    assert( err( "(program (data t (A s)))" ) == "bad data t" );
    assert( err( "(program (define x 1) (frobnicate))" ) == "syntax error" );

    return 0;
}
