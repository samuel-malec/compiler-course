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

/* Extend ‹p3_letrec› with polymorphic lists and pairs. There is no new
 * syntax: they are «built-in variables» with polymorphic types, given
 * by an initial context of type schemes (the type constructors ‹list›
 * and ‹pair› are written like ‹->›, as the first element of a list):
 *
 *     nil   : ∀a.   (list 'a)
 *     cons  : ∀a.   (-> 'a (list 'a) (list 'a))
 *     head  : ∀a.   (-> (list 'a) 'a)
 *     tail  : ∀a.   (-> (list 'a) (list 'a))
 *     null? : ∀a.   (-> (list 'a) bool)
 *     pair  : ∀a b. (-> 'a 'b (pair 'a 'b))
 *     fst   : ∀a b. (-> (pair 'a 'b) 'a)
 *     snd   : ∀a b. (-> (pair 'a 'b) 'b)
 *
 * Note that unification must now compare the «constructors» of
 * compound types, too: ‹(list 'a)› does not unify with ‹(pair 'a 'b)›,
 * and neither does ‹(pair 'a 'b)› with ‹(-> 'a 'b)›. Equality (‹=›)
 * works on lists and pairs as well (but not on functions). The
 * interface is the same as in ‹p3_letrec›. */

// std::optional< std::string > infer( std::string_view expr );

int main()
{
    assert( infer( "nil" ) == "(list 'a)" );
    assert( infer( "(cons 1 nil)" ) == "(list int)" );
    assert( infer( "(pair (cons #t nil) 5)" ) == "(pair (list bool) int)" );
    assert( infer( "(fn (p) (pair (snd p) (fst p)))" ) == "(-> (pair 'a 'b) (pair 'b 'a))" );

    std::string map = "(letrec ((map (fn (f l) (if (null? l) nil (cons (f (head l)) (map f (tail l))))))) ";
    assert( infer( map + "map)" ) == "(-> (-> 'a 'b) (list 'a) (list 'b))" );
    assert( infer( map + "(map (fn (x) (< x 3)) (cons 1 nil)))" ) == "(list bool)" );

    std::string fold = "(letrec ((foldr (fn (f z l) (if (null? l) z (f (head l) (foldr f z (tail l))))))) ";
    assert( infer( fold + "foldr)" ) == "(-> (-> 'a 'b 'b) 'b (list 'a) 'b)" );
    assert( infer( fold + "(fn (l) (foldr (fn (x n) (+ n 1)) 0 l)))" ) == "(-> (list 'a) int)" );

    std::string zip = "(letrec ((zip (fn (a b) (if (null? a) nil (cons (pair (head a) (head b)) (zip (tail a) (tail b))))))) ";
    assert( infer( zip + "zip)" ) == "(-> (list 'a) (list 'b) (list (pair 'a 'b)))" );

    assert( infer( "(fn (l) (= l (cons 1 nil)))" ) == "(-> (list int) bool)" );
    assert( infer( "(let empty nil (pair (cons 1 empty) (cons #t empty)))" ) == "(pair (list int) (list bool))" );

    assert( !infer( "(cons 1 (cons #t nil))" ) );
    assert( !infer( "(fst (cons 1 nil))" ) );
    assert( !infer( "(head (pair 1 2))" ) );
    assert( !infer( "(fn (l) (cons l l))" ) );
    assert( !infer( "((fn (p) (fst p)) (fn (x) x))" ) );

    return 0;
}
