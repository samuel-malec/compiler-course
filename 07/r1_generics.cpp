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

/* A function like the identity ‹(fn ((x int)) x)› has to be written
 * once for each type it is used with – unless the language has
 * «parametric polymorphism». In «System F» (the polymorphic
 * λ-calculus, which underlies the generics of Java, C# and Haskell),
 * a function can be abstracted over a «type»:
 *
 *     (tfn (a) (fn ((x a)) x))      ; of type (forall (a) (-> a a))
 *
 * and later «instantiated» with a concrete type: ‹(inst id int)› has
 * type ‹(-> int int)›. Extend the type checker from ‹p1_lambda› with:
 *
 *  • types: type variables (atoms other than ‹int› and ‹bool›, which
 *    must be bound by an enclosing ‹forall› or ‹tfn›), and
 *    ‹(forall (a …) τ)› with one or more distinct variables,
 *  • ‹(tfn (a …) e)› – has type ‹(forall (a …) τ)›, where ‹τ› is the
 *    type of ‹e›, checked with the type variables ‹a …› in scope,
 *  • ‹(inst e T …)› – ‹e› must have type ‹(forall (a …) τ)›, with as many
 *    variables as there are types ‹T›; the result is ‹τ› with each
 *    variable replaced by the corresponding ‹T›.
 *
 * The substitution must avoid capture (as in ‹03/p4_subst›, but on
 * types): when substituting into ‹(forall (b …) σ)› where one of the
 * ‹b›s occurs free in one of the substituted types (and the variables
 * being replaced occur free in ‹σ›), rename that ‹b› to ‹b› followed by
 * as few ‹'› as needed to make it free neither in the substituted
 * types nor in ‹σ› (consider the variables in their order in the list).
 *
 * Two types are equal if they are equal up to a consistent renaming of
 * bound type variables («α-equivalence»): ‹(forall (a) (-> a a))› and
 * ‹(forall (b) (-> b b))› are the same type. This matters when
 * checking arguments of calls, the branches of ‹if› and ‹=›.
 *
 * The function ‹type_of› returns the type of the expression, written
 * with ‹write› (without any renaming other than that required by
 * substitution), or ‹std::nullopt› if it is ill-typed. */

// std::optional< std::string > type_of( std::string_view expr );

int main()
{
    std::string id = "(tfn (a) (fn ((x a)) x))";
    std::string konst = "(tfn (a b) (fn ((x a) (y b)) x))";

    assert( type_of( id ) == "(forall (a) (-> a a))" );
    assert( type_of( "(inst " + id + " int)" ) == "(-> int int)" );
    assert( type_of( "((inst " + id + " int) 5)" ) == "int" );
    assert( type_of( "(inst " + id + " (-> bool bool))" ) == "(-> (-> bool bool) (-> bool bool))" );
    assert( type_of( konst ) == "(forall (a b) (-> a b a))" );
    assert( type_of( "((inst " + konst + " bool int) #t 7)" ) == "bool" );
    assert( type_of( "(tfn (a) (fn ((f (-> a a)) (x a)) (f (f x))))" ) ==
            "(forall (a) (-> (-> a a) a a))" );
    assert( type_of( "(inst (tfn (a) " + id + ") int)" ) == "(forall (a) (-> a a))" );

    /* capture-avoiding substitution */
    assert( type_of( "(tfn (b) (inst (tfn (a) (tfn (b) (fn ((x a) (y b)) x))) b))" ) ==
            "(forall (b) (forall (b') (-> b b' b)))" );

    /* α-equivalence: a polymorphic argument */
    assert( type_of( "((fn ((g (forall (b) (-> b b)))) (inst g int)) " + id + ")" ) == "(-> int int)" );
    assert( type_of( "(if #t " + id + " (tfn (z) (fn ((w z)) w)))" ) == "(forall (a) (-> a a))" );

    assert( !type_of( "(fn ((x a)) x)" ) );
    assert( !type_of( "(inst 5 int)" ) );
    assert( !type_of( "(inst " + id + " int bool)" ) );
    assert( !type_of( "((inst " + id + " int) #t)" ) );
    assert( !type_of( "(tfn (a a) 1)" ) );
    assert( !type_of( "((fn ((g (forall (b) (-> b int)))) 1) " + id + ")" ) );
    assert( !type_of( "(inst " + id + " c)" ) );

    return 0;
}
