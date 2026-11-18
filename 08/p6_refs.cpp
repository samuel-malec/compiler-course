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

/* Extend ‹p3_letrec› with mutable references:
 *
 *  • ‹(ref e)› – a new cell holding ‹e›; type ‹(ref T)› if ‹e : T›,
 *  • ‹(get r)› – the content of the cell; ‹T› if ‹r : (ref T)›,
 *  • ‹(put r e)› – store ‹e› into ‹r› (‹e› must have type ‹T›, where
 *    ‹r : (ref T)›); the type of the expression is ‹T›.
 *
 * Unrestricted let-polymorphism is «unsound» with references:
 *
 *     (let r (ref (fn (x) x))             ; r : ∀a. (ref (-> 'a 'a)) ?!
 *       (let u (put r (fn (n) (+ n 1)))   ; use r at (ref (-> int int))
 *         ((get r) #t)))                  ; use r at (ref (-> bool bool))
 *
 * would type-check, and then add 1 to ‹#t›. The standard fix is the
 * «value restriction»: generalize the type of ‹(let x e b)› only if
 * ‹e› is a «syntactic value» – a literal, a variable or a ‹fn›
 * expression – which cannot create a reference. Otherwise, ‹x› gets a
 * monomorphic type (whose variables can still be determined by later
 * uses). Implement it. (The group of a ‹letrec› always consists of
 * ‹fn›s, so it is generalized as before.) The interface is the same as
 * in ‹p3_letrec›. */

// std::optional< std::string > infer( std::string_view expr );

int main()
{
    assert( infer( "(ref 1)" ) == "(ref int)" );
    assert( infer( "(fn (r) (put r (+ (get r) 1)))" ) == "(-> (ref int) int)" );
    assert( infer( "(let r (ref (fn (x) x)) (let u (put r (fn (n) (+ n 1))) (get r)))" ) == "(-> int int)" );
    assert( !infer( "(let r (ref (fn (x) x)) (let u (put r (fn (n) (+ n 1))) ((get r) #t)))" ) );

    /* values are still generalized */
    assert( infer( "(let id (fn (x) x) (if (id #t) (id 1) 0))" ) == "int" );
    assert( infer( "(let mk (fn (x) (ref x)) (let a (mk 1) (let b (mk #t) (if (get b) (get a) 0))))" ) == "int" );

    /* … but an application is not a value, even if it would be safe */
    assert( infer( "(let f ((fn (x) x) (fn (y) y)) (f 1))" ) == "int" );
    assert( !infer( "(let f ((fn (x) x) (fn (y) y)) (if (f #t) (f 1) 0))" ) );

    /* a monomorphic variable is determined by its uses */
    assert( infer( "(let r (ref (fn (x) x)) r)" ) == "(ref (-> 'a 'a))" );
    assert( infer( "(fn (v) (let c (ref v) (get c)))" ) == "(-> 'a 'a)" );

    assert( !infer( "(get 1)" ) );
    assert( !infer( "(put (ref 1) #t)" ) );

    return 0;
}
