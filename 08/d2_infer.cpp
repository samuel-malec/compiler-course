#include <cassert>
#include <map>
#include <set>
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

/* This demo implements «Algorithm W»: Hindley–Milner type inference
 * with let-polymorphism, for the language from the introduction. The
 * substitution and unification are those of ‹d1_unify›. */

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
type fun( std::vector< type > ts ) { ts.insert( ts.begin(), atom( "->" ) ); return sexp{ "", ts, 0 }; }
bool is_var( const type &t ) { return t.is_atom() && t.atom[ 0 ] == '\''; }

struct type_error : std::runtime_error { using std::runtime_error::runtime_error; };

/* A «type scheme» is a type with some of its variables quantified:
 * ‹∀'a. (-> 'a 'a)›. A monomorphic type is a scheme with no
 * quantified variables. The context maps names to schemes. */

struct scheme
{
    std::set< std::string > vars;
    type body;
};

using context = std::map< std::string, scheme >;

struct inference
{
    std::map< std::string, type > subst;
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
        for ( auto &x : out.items )
            x = apply( x );
        return out;
    }

    void free_vars( const type &t, std::set< std::string > &out ) const
    {
        type u = apply( t );
        if ( is_var( u ) ) out.insert( u.atom );
        for ( const auto &x : u.items ) free_vars( x, out );
    }

    void unify( const type &a0, const type &b0 )
    {
        type a = apply( a0 ), b = apply( b0 );
        if ( is_var( a ) && is_var( b ) && a.atom == b.atom )
            return;
        if ( is_var( a ) || is_var( b ) )
        {
            const type &v = is_var( a ) ? a : b, &t = is_var( a ) ? b : a;
            std::set< std::string > fv;
            free_vars( t, fv );
            if ( fv.contains( v.atom ) )
                throw type_error( "infinite type" );
            subst.emplace( v.atom, t );
            return;
        }
        if ( a.is_atom() || b.is_atom() || a.items.size() != b.items.size() )
        {
            if ( a.is_atom() && b.is_atom() && a.atom == b.atom )
                return;
            throw type_error( "cannot unify " + write( a ) + " with " + write( b ) );
        }
        for ( std::size_t i = 1; i < a.items.size(); ++ i )
            unify( a.items[ i ], b.items[ i ] );
    }

    /* «Instantiate» a scheme: replace the quantified variables by fresh
     * ones. Every use of a let-bound variable does this, so each use
     * gets its own copy of the type. */
    type instantiate( const scheme &s )
    {
        std::map< std::string, type > fresh_vars;
        for ( const auto &v : s.vars )
            fresh_vars.emplace( v, fresh() );

        auto copy = [ & ]( auto &self, const type &t ) -> type
        {
            if ( is_var( t ) && fresh_vars.contains( t.atom ) )
                return fresh_vars.at( t.atom );
            type out = t;
            for ( auto &x : out.items )
                x = self( self, x );
            return out;
        };
        return copy( copy, apply( s.body ) );
    }

    /* «Generalize» a type: quantify the variables which are free in the
     * type, but not in the context. (A variable which is free in the
     * context belongs to some enclosing ‹fn› parameter; it is not ours
     * to quantify – the parameter may later turn out to be an ‹int›.) */
    scheme generalize( const context &ctx, const type &t ) const
    {
        std::set< std::string > in_type, in_ctx;
        free_vars( t, in_type );
        for ( const auto &[ name, s ] : ctx )
        {
            std::set< std::string > fv;
            free_vars( s.body, fv );
            for ( const auto &v : s.vars ) fv.erase( v );
            in_ctx.merge( fv );
        }
        scheme out{ {}, apply( t ) };
        for ( const auto &v : in_type )
            if ( !in_ctx.contains( v ) )
                out.vars.insert( v );
        return out;
    }

    type infer( const sexp &e, const context &ctx )
    {
        const type int_t = atom( "int" ), bool_t = atom( "bool" );

        if ( e.is_atom() )
        {
            if ( e.atom == "#t" || e.atom == "#f" ) return bool_t;
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos ) return int_t;
            if ( auto it = ctx.find( e.atom ); it != ctx.end() )
                return instantiate( it->second );
            throw type_error( "unbound variable " + e.atom );
        }

        const auto &it = e.items;
        const std::string &h = it.at( 0 ).atom;

        if ( h == "+" || h == "-" || h == "*" || h == "/" || h == "<" || h == "<=" || h == ">" || h == ">=" )
        {
            unify( infer( it.at( 1 ), ctx ), int_t );
            unify( infer( it.at( 2 ), ctx ), int_t );
            return h.size() == 1 && h != "<" && h != ">" ? int_t : bool_t;
        }

        if ( h == "=" || h == "!=" )
        {
            type a = infer( it.at( 1 ), ctx );
            unify( a, infer( it.at( 2 ), ctx ) );
            if ( apply( a ).is_list() )
                throw type_error( "cannot compare functions" );
            return bool_t;
        }

        if ( h == "and" || h == "or" || h == "not" )
        {
            for ( std::size_t i = 1; i < it.size(); ++ i )
                unify( infer( it[ i ], ctx ), bool_t );
            return bool_t;
        }

        if ( h == "if" )
        {
            unify( infer( it.at( 1 ), ctx ), bool_t );
            type t = infer( it.at( 2 ), ctx );
            unify( t, infer( it.at( 3 ), ctx ) );
            return t;
        }

        /* ‹fn›: each parameter gets a fresh type variable, «not»
         * generalized – within the body, it is one fixed (if unknown)
         * type. */
        if ( h == "fn" )
        {
            context inner = ctx;
            std::vector< type > ts;
            for ( const auto &p : it.at( 1 ).items )
            {
                ts.push_back( fresh() );
                inner.insert_or_assign( p.atom, scheme{ {}, ts.back() } );
            }
            ts.push_back( infer( it.at( 2 ), inner ) );
            return fun( ts );
        }

        /* ‹let›: infer the bound expression, «generalize», and infer the
         * body with the scheme. */
        if ( h == "let" )
        {
            type t = infer( it.at( 2 ), ctx );
            context inner = ctx;
            inner.insert_or_assign( it.at( 1 ).atom, generalize( ctx, t ) );
            return infer( it.at( 3 ), inner );
        }

        /* A call: the callee must be a function from the argument types
         * to some result type, which we don't know yet – a fresh
         * variable. */
        type f = infer( it[ 0 ], ctx );
        std::vector< type > ts;
        for ( std::size_t i = 1; i < it.size(); ++ i )
            ts.push_back( infer( it[ i ], ctx ) );
        type result = fresh();
        ts.push_back( result );
        unify( f, fun( ts ) );
        return result;
    }
};

/* Rename the variables of a type to ‹'a›, ‹'b›, … in order of their
 * first appearance (see the introduction). */

type normalize( const type &t, std::map< std::string, std::string > &names )
{
    if ( is_var( t ) )
    {
        if ( !names.contains( t.atom ) )
            names[ t.atom ] = std::string( "'" ) + char( 'a' + names.size() );
        return atom( names[ t.atom ] );
    }
    type out = t;
    for ( auto &x : out.items )
        x = normalize( x, names );
    return out;
}

std::string type_of( std::string_view src )
{
    try
    {
        inference w;
        std::map< std::string, std::string > names;
        return write( normalize( w.apply( w.infer( read( src ), {} ) ), names ) );
    }
    catch ( const type_error &e ) { return e.what(); }
}

int main() /* demo */
{
    assert( type_of( "(fn (x) (+ x 1))" ) == "(-> int int)" );
    assert( type_of( "(fn (x) x)" ) == "(-> 'a 'a)" );
    assert( type_of( "(fn (f x) (f x))" ) == "(-> (-> 'a 'b) 'a 'b)" );
    assert( type_of( "(fn (f g x) (f (g x)))" ) == "(-> (-> 'a 'b) (-> 'c 'a) 'c 'b)" );
    assert( type_of( "(fn (x y) (if (= x y) x y))" ) == "(-> 'a 'a 'a)" );
    assert( type_of( "(fn (f) (fn (x) (f (f x))))" ) == "(-> (-> 'a 'a) (-> 'a 'a))" );

    /* let-polymorphism: ‹id› is used at two different types */
    assert( type_of( "(let id (fn (x) x) (if (id #t) (id 1) 0))" ) == "int" );

    /* … which does not work for a parameter */
    assert( type_of( "((fn (id) (if (id #t) (id 1) 0)) (fn (x) x))" ) == "cannot unify bool with int" );

    /* generalization must not touch variables of the context: here ‹y›
     * belongs to the enclosing ‹fn›, so ‹g› is not polymorphic in it */
    assert( type_of( "(fn (y) (let g (fn (z) y) (+ (g 1) (g #t))))" ) == "(-> int int)" );

    assert( type_of( "(fn (x) (x x))" ) == "infinite type" );
    assert( type_of( "(+ 1 #t)" ) == "cannot unify bool with int" );
    assert( type_of( "(= (fn (x) x) (fn (y) y))" ) == "cannot compare functions" );

    return 0;
}
