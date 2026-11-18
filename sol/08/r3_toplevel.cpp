#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <functional>

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


/* Free variables of an expression (keywords and operators excluded). */
void free_names( const sexp &e, std::set< std::string > bound, std::set< std::string > &out )
{
    if ( e.is_atom() )
    {
        if ( e.atom.find_first_not_of( "-0123456789" ) != e.atom.npos && e.atom != "#t" &&
             e.atom != "#f" && !bound.contains( e.atom ) )
            out.insert( e.atom );
        return;
    }
    if ( e.items.empty() ) return;
    const std::string &h = e.items[ 0 ].is_atom() ? e.items[ 0 ].atom : "";
    static const std::set< std::string > ops =
        { "+", "-", "*", "/", "<", "<=", ">", ">=", "=", "!=", "and", "or", "not", "if" };

    if ( h == "fn" && e.items.size() == 3 )
    {
        for ( const auto &p : e.items[ 1 ].items ) bound.insert( p.atom );
        free_names( e.items[ 2 ], bound, out );
    }
    else if ( h == "let" && e.items.size() == 4 )
    {
        free_names( e.items[ 2 ], bound, out );
        bound.insert( e.items[ 1 ].atom );
        free_names( e.items[ 3 ], bound, out );
    }
    else if ( h == "letrec" && e.items.size() == 3 )
    {
        for ( const auto &b : e.items[ 1 ].items ) bound.insert( b.items.at( 0 ).atom );
        for ( const auto &b : e.items[ 1 ].items ) free_names( b.items.at( 1 ), bound, out );
        free_names( e.items[ 2 ], bound, out );
    }
    else
        for ( std::size_t i = ops.contains( h ) ? 1 : 0; i < e.items.size(); ++ i )
            free_names( e.items[ i ], bound, out );
}

std::optional< std::vector< std::string > > infer_program( std::string_view program )
{
    try
    {
        sexp p = read( "(" + std::string( program ) + ")" );
        std::vector< std::string > names;
        std::map< std::string, std::size_t > index;
        for ( const auto &d : p.items )
        {
            if ( !d.is_list() || d.items.size() != 3 || d.items[ 0 ].atom != "define" || !d.items[ 1 ].is_atom() ||
                 !index.emplace( d.items[ 1 ].atom, names.size() ).second )
                return std::nullopt;
            names.push_back( d.items[ 1 ].atom );
        }

        std::size_t n = names.size();
        std::vector< std::vector< std::size_t > > deps( n );
        for ( std::size_t i = 0; i < n; ++ i )
        {
            std::set< std::string > fv;
            free_names( p.items[ i ].items[ 2 ], {}, fv );
            for ( const auto &v : fv )
                if ( index.contains( v ) ) deps[ i ].push_back( index.at( v ) );
        }

        /* Tarjan's algorithm; components come out dependencies first */
        std::vector< int > idx( n, -1 ), low( n, 0 );
        std::vector< bool > on_stack( n, false );
        std::vector< std::size_t > stack;
        std::vector< std::vector< std::size_t > > comps;
        int counter = 0;

        std::function< void( std::size_t ) > visit = [ & ]( std::size_t v )
        {
            idx[ v ] = low[ v ] = counter++;
            stack.push_back( v ), on_stack[ v ] = true;
            for ( auto w : deps[ v ] )
                if ( idx[ w ] < 0 ) visit( w ), low[ v ] = std::min( low[ v ], low[ w ] );
                else if ( on_stack[ w ] ) low[ v ] = std::min( low[ v ], idx[ w ] );
            if ( low[ v ] == idx[ v ] )
            {
                comps.emplace_back();
                std::size_t w;
                do
                    w = stack.back(), stack.pop_back(), on_stack[ w ] = false, comps.back().push_back( w );
                while ( w != v );
            }
        };
        for ( std::size_t v = 0; v < n; ++ v )
            if ( idx[ v ] < 0 ) visit( v );

        inference w;
        context ctx = builtins();
        std::vector< std::string > out( n );

        for ( const auto &comp : comps )
        {
            context group = ctx;
            std::map< std::size_t, type > self;
            for ( auto d : comp )
                group.insert_or_assign( names[ d ], scheme{ {}, self[ d ] = w.fresh() } );
            for ( auto d : comp )
                w.unify( w.infer( p.items[ d ].items[ 2 ], group ), self[ d ], 0 );
            context next = ctx;
            for ( auto d : comp )
                next.insert_or_assign( names[ d ], w.generalize( ctx, self[ d ] ) );
            ctx = next;
            for ( auto d : comp )
            {
                std::map< std::string, std::string > nm;
                out[ d ] = names[ d ] + " : " + write( normalize( w.apply( self[ d ] ), nm ) );
            }
        }

        return out;
    }
    catch ( const type_error & ) { return std::nullopt; }
}

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
