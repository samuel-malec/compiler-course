#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>

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

/* Type errors carry the offset of the offending expression and a
 * message (only some exercises use them). */
struct type_error { std::size_t offset; std::string text; };

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
type ctor( std::vector< type > ts ) { return sexp{ "", std::move( ts ), 0 }; }
bool is_var( const type &t ) { return t.is_atom() && t.atom[ 0 ] == '\''; }

struct scheme { std::set< std::string > vars; type body; };
using context = std::map< std::string, scheme >;

type normalize( const type &t, std::map< std::string, std::string > &names )
{
    if ( is_var( t ) )
    {
        if ( !names.contains( t.atom ) )
            names[ t.atom ] = std::string( "'" ) + char( 'a' + names.size() );
        return atom( names[ t.atom ] );
    }
    type out = t;
    for ( auto &x : out.items ) x = normalize( x, names );
    return out;
}

struct inference
{
    std::map< std::string, type > subst = {};
    int counter = 0;

    type fresh() { return atom( "'t" + std::to_string( counter++ ) ); }

    type apply( const type &t ) const
    {
        if ( is_var( t ) )
        {
            auto it = subst.find( t.atom );
            return it == subst.end() ? t : apply( it->second );
        }
        type out = t;
        for ( auto &x : out.items ) x = apply( x );
        return out;
    }

    void free_vars( const type &t, std::set< std::string > &out ) const
    {
        type u = apply( t );
        if ( is_var( u ) ) out.insert( u.atom );
        for ( const auto &x : u.items ) free_vars( x, out );
    }

    /* Returns false if the types cannot be unified; ‹infinite› tells
     * whether the reason was the occurs check. */
    bool unify_( const type &a0, const type &b0, bool &infinite )
    {
        type a = apply( a0 ), b = apply( b0 );
        if ( is_var( a ) && is_var( b ) && a.atom == b.atom ) return true;
        if ( is_var( a ) || is_var( b ) )
        {
            const type &v = is_var( a ) ? a : b, &t = is_var( a ) ? b : a;
            std::set< std::string > fv;
            free_vars( t, fv );
            if ( fv.contains( v.atom ) ) return infinite = true, false;
            subst.emplace( v.atom, t );
            return true;
        }
        if ( a.is_atom() || b.is_atom() )
            return a.is_atom() && b.is_atom() && a.atom == b.atom;
        if ( a.items.size() != b.items.size() ) return false;
        for ( std::size_t i = 0; i < a.items.size(); ++ i )
            if ( !unify_( a.items[ i ], b.items[ i ], infinite ) ) return false;
        return true;
    }

    /* Unify ‹a› (the type of the expression at ‹where›) with ‹b›. */
    void unify( const type &a, const type &b, std::size_t where )
    {
        bool infinite = false;
        type sa = apply( a ), sb = apply( b ); /* for the message */
        if ( unify_( a, b, infinite ) ) return;
        if ( infinite ) throw type_error{ where, "infinite type" };
        std::map< std::string, std::string > names;
        type na = normalize( sa, names ), nb = normalize( sb, names );
        throw type_error{ where, "cannot unify " + write( na ) + " with " + write( nb ) };
    }

    type instantiate( const scheme &s )
    {
        std::map< std::string, type > m;
        for ( const auto &v : s.vars ) m.emplace( v, fresh() );
        auto copy = [ & ]( auto &self, const type &t ) -> type
        {
            if ( is_var( t ) && m.contains( t.atom ) ) return m.at( t.atom );
            type out = t;
            for ( auto &x : out.items ) x = self( self, x );
            return out;
        };
        return copy( copy, apply( s.body ) );
    }

    std::set< std::string > context_vars( const context &ctx ) const
    {
        std::set< std::string > out;
        for ( const auto &[ _, s ] : ctx )
        {
            std::set< std::string > fv;
            free_vars( s.body, fv );
            for ( const auto &v : s.vars ) fv.erase( v );
            out.merge( fv );
        }
        return out;
    }

    scheme generalize( const context &ctx, const type &t ) const
    {
        std::set< std::string > in_type;
        free_vars( t, in_type );
        auto in_ctx = context_vars( ctx );
        scheme out{ {}, apply( t ) };
        for ( const auto &v : in_type )
            if ( !in_ctx.contains( v ) ) out.vars.insert( v );
        return out;
    }

    static scheme mono( const type &t ) { return { {}, t }; }

    [[noreturn]] static void bad( const sexp &e ) { throw type_error{ e.offset, "bad form" }; }

    static bool is_value( const sexp &e )
    {
        return e.is_atom() || ( !e.items.empty() && e.items[ 0 ].atom == "fn" );
    }

    type infer( const sexp &e, const context &ctx )
    {
        const type int_t = atom( "int" ), bool_t = atom( "bool" );

        if ( e.is_atom() )
        {
            if ( e.atom == "#t" || e.atom == "#f" ) return bool_t;
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos && e.atom != "-" ) return int_t;
            if ( auto it = ctx.find( e.atom ); it != ctx.end() ) return instantiate( it->second );
            throw type_error{ e.offset, "unbound variable " + e.atom };
        }

        const auto &it = e.items;
        if ( it.empty() ) bad( e );
        const std::string h = it[ 0 ].is_atom() ? it[ 0 ].atom : "";
        auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) bad( e ); };
        auto expect = [ & ]( const sexp &x, const type &t ) { unify( infer( x, ctx ), t, x.offset ); };

        if ( h == "+" || h == "-" || h == "*" || h == "/" || h == "<" || h == "<=" || h == ">" || h == ">=" )
        {
            arity( 2 );
            expect( it[ 1 ], int_t );
            expect( it[ 2 ], int_t );
            return ( h == "+" || h == "-" || h == "*" || h == "/" ) ? int_t : bool_t;
        }
        if ( h == "=" || h == "!=" )
        {
            arity( 2 );
            type a = infer( it[ 1 ], ctx );
            if ( apply( a ).is_list() && apply( a ).items[ 0 ].atom == "->" )
            {
                std::map< std::string, std::string > names;
                throw type_error{ it[ 1 ].offset, "cannot compare " + write( normalize( apply( a ), names ) ) };
            }
            expect( it[ 2 ], a );
            type r = apply( a );
            if ( r.is_list() && r.items[ 0 ].atom == "->" )
            {
                std::map< std::string, std::string > names;
                throw type_error{ it[ 1 ].offset, "cannot compare " + write( normalize( r, names ) ) };
            }
            return bool_t;
        }
        if ( h == "and" || h == "or" || h == "not" )
        {
            arity( h == "not" ? 1 : 2 );
            for ( std::size_t i = 1; i < it.size(); ++ i ) expect( it[ i ], bool_t );
            return bool_t;
        }
        if ( h == "if" )
        {
            arity( 3 );
            expect( it[ 1 ], bool_t );
            type t = infer( it[ 2 ], ctx );
            expect( it[ 3 ], t );
            return t;
        }
        if ( h == "fn" )
        {
            arity( 2 );
            if ( !it[ 1 ].is_list() ) bad( e );
            context inner = ctx;
            std::vector< type > ts{ atom( "->" ) };
            std::set< std::string > seen;
            for ( const auto &p : it[ 1 ].items )
            {
                if ( !p.is_atom() || !seen.insert( p.atom ).second ) bad( e );
                ts.push_back( fresh() );
                inner.insert_or_assign( p.atom, mono( ts.back() ) );
            }
            ts.push_back( infer( it[ 2 ], inner ) );
            return ctor( ts );
        }
        if ( h == "let" )
        {
            arity( 3 );
            if ( !it[ 1 ].is_atom() ) bad( e );
            type t = infer( it[ 2 ], ctx );
            context inner = ctx;
            inner.insert_or_assign( it[ 1 ].atom, generalize( ctx, t ) );
            return infer( it[ 3 ], inner );
        }
        if ( h == "letrec" )
        {
            arity( 2 );
            if ( !it[ 1 ].is_list() ) bad( e );
            context group = ctx;
            std::vector< type > self;
            std::set< std::string > names;
            for ( const auto &b : it[ 1 ].items )
            {
                if ( !b.is_list() || b.items.size() != 2 || !b.items[ 0 ].is_atom() ||
                     !b.items[ 1 ].is_list() || b.items[ 1 ].items.empty() ||
                     b.items[ 1 ].items[ 0 ].atom != "fn" || !names.insert( b.items[ 0 ].atom ).second )
                    bad( e );
                self.push_back( fresh() );
                group.insert_or_assign( b.items[ 0 ].atom, mono( self.back() ) );
            }
            for ( std::size_t i = 0; i < self.size(); ++ i )
                unify( infer( it[ 1 ].items[ i ].items[ 1 ], group ), self[ i ], it[ 1 ].items[ i ].offset );
            context inner = ctx;
            for ( std::size_t i = 0; i < self.size(); ++ i )
                inner.insert_or_assign( it[ 1 ].items[ i ].items[ 0 ].atom, generalize( ctx, self[ i ] ) );
            return infer( it[ 2 ], inner );
        }

        type f = infer( it[ 0 ], ctx );
        std::vector< type > ts{ atom( "->" ) };
        for ( std::size_t i = 1; i < it.size(); ++ i )
            ts.push_back( infer( it[ i ], ctx ) );
        type r = fresh();
        ts.push_back( r );
        unify( f, ctor( ts ), e.offset );
        return r;
    }
};

context builtins()
{
    context c;
    auto add = [ & ]( std::string name, std::string t )
    {
        type ty = read( t );
        std::set< std::string > fv;
        inference{}.free_vars( ty, fv );
        c.emplace( name, scheme{ fv, ty } );
    };
    add( "nil", "(list 'a)" );
    add( "cons", "(-> 'a (list 'a) (list 'a))" );
    add( "head", "(-> (list 'a) 'a)" );
    add( "tail", "(-> (list 'a) (list 'a))" );
    add( "null?", "(-> (list 'a) bool)" );
    add( "pair", "(-> 'a 'b (pair 'a 'b))" );
    add( "fst", "(-> (pair 'a 'b) 'a)" );
    add( "snd", "(-> (pair 'a 'b) 'b)" );
    return c;
}

std::optional< std::string > infer( std::string_view expr )
{
    try
    {
        inference w;
        std::map< std::string, std::string > names;
        return write( normalize( w.apply( w.infer( read( expr ), builtins() ) ), names ) );
    }
    catch ( const type_error & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

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
