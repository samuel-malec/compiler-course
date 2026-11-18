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

/* Write a type inference function for the language from the
 * introduction, «without» let-polymorphism: ‹(let x e b)› is treated
 * like ‹((fn (x) b) e)› – the variable ‹x› gets the type of ‹e›, and
 * that type is the same at all uses of ‹x›. Every other construct
 * behaves as in ‹d2_infer›. The function ‹infer› returns the principal
 * type of the expression, in normal form (see the introduction), or
 * ‹std::nullopt› if the expression has no type or is malformed. */

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
            inner.insert_or_assign( it[ 1 ].atom, mono( t ) );
            return infer( it[ 3 ], inner );
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

std::optional< std::string > infer( std::string_view expr )
{
    try
    {
        inference w;
        std::map< std::string, std::string > names;
        return write( normalize( w.apply( w.infer( read( expr ), {} ) ), names ) );
    }
    catch ( const type_error & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    assert( infer( "1" ) == "int" );
    assert( infer( "(fn (x) x)" ) == "(-> 'a 'a)" );
    assert( infer( "(fn (x y) x)" ) == "(-> 'a 'b 'a)" );
    assert( infer( "(fn (x) (if x 1 2))" ) == "(-> bool int)" );
    assert( infer( "(fn (f) (f 1))" ) == "(-> (-> int 'a) 'a)" );
    assert( infer( "(fn (f g x) (g (f x)))" ) == "(-> (-> 'a 'b) (-> 'b 'c) 'a 'c)" );
    assert( infer( "(fn (x y) (and (= x y) (< y 2)))" ) == "(-> int int bool)" );
    assert( infer( "(fn (x y) (= x y))" ) == "(-> 'a 'a bool)" );
    assert( infer( "(let id (fn (x) x) (id 1))" ) == "int" );
    assert( infer( "(fn (k) (fn (x) (fn (y) ((k y) x))))" ) == "(-> (-> 'a (-> 'b 'c)) (-> 'b (-> 'a 'c)))" );
    assert( infer( "(fn () 5)" ) == "(-> int)" );
    assert( infer( "((fn (f) (f)) (fn () #t))" ) == "bool" );

    assert( !infer( "(let id (fn (x) x) (if (id #t) (id 1) 0))" ) ); /* no polymorphism */
    assert( !infer( "(fn (x) (x x))" ) );
    assert( !infer( "(+ 1 #f)" ) );
    assert( !infer( "((fn (x) x) 1 2)" ) );
    assert( !infer( "(= (fn (x) x) (fn (x) x))" ) );
    assert( !infer( "(fn (f) (= f (fn (x) 1)))" ) );
    assert( !infer( "z" ) );
    assert( !infer( "(if 1)" ) );

    return 0;
}
