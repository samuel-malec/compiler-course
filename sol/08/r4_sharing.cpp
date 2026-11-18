#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>

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

/* The principal type of an expression can be «exponentially» larger
 * than the expression itself. Consider (with ‹pair› as in ‹p4_lists›):
 *
 *     (let f0 (fn (x) (pair x x))
 *       (let f1 (fn (y) (f0 (f0 y)))
 *         (let f2 (fn (y) (f1 (f1 y)))  …
 *
 * The type of ‹(f0 1)› is ‹(pair int int)›, that of ‹(f1 1)› is a pair of
 * pairs of pairs, and every further level doubles the depth. Printing
 * such types is hopeless, but inferring them is not – if they are
 * represented as «DAGs», where identical sub-terms are shared rather
 * than copied: ‹(pair T T)› needs only one copy of ‹T›.
 *
 * Write a function ‹type_size› which infers the type of an expression
 * in the language of ‹p4_lists› (with let-polymorphism, ‹letrec› and
 * the built-ins) and returns its «size»: the number of nodes of the
 * type «written out as a tree» (every atom – ‹int›, ‹bool›, a type
 * variable – counts as 1, and every compound type ‹(c T₁ … Tₙ)› as 1
 * plus the sizes of ‹T₁ … Tₙ›; the constructor name itself does not
 * count). Return ‹std::nullopt› if the expression has no type.
 *
 * The tests contain types with more than ⟦2⁴⁰⟧ nodes. To handle them,
 * the whole inference must work on shared representations: types as
 * nodes referring to other nodes (‹std::shared_ptr›, or indices into a
 * vector), type variables as mutable nodes which get «bound» by
 * unification (a union-find, see ‹r2_unionfind›), instantiation which
 * copies a scheme while preserving its sharing (remember which nodes
 * have already been copied), and a size computation which memoizes
 * the sizes of nodes. Unification of two such types must also avoid
 * comparing the same pair of nodes repeatedly (mark pairs which are
 * already unified – or simply bind the variables as you go, so that
 * a second visit of the same pair finds them identical). */

// std::optional< long > type_size( std::string_view expr );

struct type_error {};

/* A type is a graph of nodes. A variable has an empty ‹con›; any node
 * can be «linked» to another one (union-find), which means it has been
 * unified with it and stands for the same type. Compound nodes are
 * linked too: once two compound nodes have been unified, they are one
 * node, and unifying them again is free. */

struct tnode
{
    std::string con;
    std::vector< tnode * > args;
    tnode *link = nullptr;
};

struct scheme
{
    std::unordered_set< const tnode * > generic;
    tnode *body;
};

using context = std::map< std::string, scheme >;

struct dag_inference
{
    std::vector< std::unique_ptr< tnode > > arena;
    tnode *int_t = mk( "int" ), *bool_t = mk( "bool" );

    tnode *mk( std::string con, std::vector< tnode * > args = {} )
    {
        arena.push_back( std::make_unique< tnode >( tnode{ std::move( con ), std::move( args ) } ) );
        return arena.back().get();
    }

    tnode *fresh() { return mk( "" ); }

    tnode *find( tnode *t )
    {
        tnode *root = t;
        while ( root->link ) root = root->link;
        while ( t->link ) { tnode *next = t->link; t->link = root; t = next; }
        return root;
    }

    /* does the variable ‹v› occur in ‹t›? (each node visited once) */
    bool occurs( tnode *v, tnode *t, std::unordered_set< tnode * > &seen )
    {
        t = find( t );
        if ( t == v ) return true;
        if ( !seen.insert( t ).second ) return false;
        for ( auto *c : t->args )
            if ( occurs( v, c, seen ) ) return true;
        return false;
    }

    void unify( tnode *a, tnode *b )
    {
        a = find( a ), b = find( b );
        if ( a == b ) return;

        if ( a->con.empty() || b->con.empty() )
        {
            tnode *v = a->con.empty() ? a : b, *t = a->con.empty() ? b : a;
            std::unordered_set< tnode * > seen;
            if ( occurs( v, t, seen ) ) throw type_error();
            v->link = t;
            return;
        }

        if ( a->con != b->con || a->args.size() != b->args.size() )
            throw type_error();

        a->link = b; /* link first, so that cycles of pairs terminate */
        for ( std::size_t i = 0; i < a->args.size(); ++ i )
            unify( a->args[ i ], b->args[ i ] );
    }

    void free_vars( tnode *t, std::unordered_set< tnode * > &out, std::unordered_set< tnode * > &seen,
                    const std::unordered_set< const tnode * > *except = nullptr )
    {
        t = find( t );
        if ( !seen.insert( t ).second ) return;
        if ( t->con.empty() )
        {
            if ( !except || !except->contains( t ) ) out.insert( t );
            return;
        }
        for ( auto *c : t->args ) free_vars( c, out, seen, except );
    }

    scheme generalize( const context &ctx, tnode *t )
    {
        std::unordered_set< tnode * > in_ctx, seen;
        for ( const auto &[ _, s ] : ctx )
            free_vars( s.body, in_ctx, seen, &s.generic );

        std::unordered_set< tnode * > in_t, seen_t;
        free_vars( t, in_t, seen_t );

        scheme s{ {}, t };
        for ( auto *v : in_t )
            if ( !in_ctx.contains( v ) ) s.generic.insert( v );
        return s;
    }

    /* Copy the scheme body, replacing generic variables by fresh ones.
     * The memo makes sure that each node is copied only once, so the
     * copy has the same sharing as the original; nodes which do not
     * contain any generic variable are not copied at all. */
    tnode *instantiate( const scheme &s )
    {
        if ( s.generic.empty() ) return s.body;
        std::unordered_map< tnode *, tnode * > memo;

        auto copy = [ & ]( auto &self, tnode *t ) -> tnode *
        {
            t = find( t );
            if ( auto it = memo.find( t ); it != memo.end() ) return it->second;
            tnode *r = t;
            if ( t->con.empty() )
                r = s.generic.contains( t ) ? fresh() : t;
            else
            {
                std::vector< tnode * > args;
                bool changed = false;
                for ( auto *c : t->args )
                {
                    args.push_back( self( self, c ) );
                    changed = changed || args.back() != find( c );
                }
                if ( changed ) r = mk( t->con, args );
            }
            return memo[ t ] = r;
        };

        return copy( copy, s.body );
    }

    tnode *from_sexp( const sexp &t, std::map< std::string, tnode * > &vars )
    {
        if ( t.is_atom() )
        {
            if ( t.atom[ 0 ] == '\'' )
                return vars.contains( t.atom ) ? vars[ t.atom ] : vars[ t.atom ] = fresh();
            return t.atom == "int" ? int_t : t.atom == "bool" ? bool_t : mk( t.atom );
        }
        std::vector< tnode * > args;
        for ( std::size_t i = 1; i < t.items.size(); ++ i )
            args.push_back( from_sexp( t.items[ i ], vars ) );
        return mk( t.items[ 0 ].atom, args );
    }

    context builtins()
    {
        context c;
        auto add = [ & ]( std::string name, std::string t )
        {
            std::map< std::string, tnode * > vars;
            scheme s{ {}, from_sexp( read( t ), vars ) };
            for ( auto &[ _, v ] : vars ) s.generic.insert( v );
            c.emplace( name, s );
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

    tnode *infer( const sexp &e, const context &ctx )
    {
        if ( e.is_atom() )
        {
            if ( e.atom == "#t" || e.atom == "#f" ) return bool_t;
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos && e.atom != "-" ) return int_t;
            if ( auto it = ctx.find( e.atom ); it != ctx.end() ) return instantiate( it->second );
            throw type_error();
        }

        const auto &it = e.items;
        if ( it.empty() ) throw type_error();
        const std::string h = it[ 0 ].is_atom() ? it[ 0 ].atom : "";
        auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw type_error(); };
        auto expect = [ & ]( const sexp &x, tnode *t ) { unify( infer( x, ctx ), t ); };

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
            tnode *a = infer( it[ 1 ], ctx );
            expect( it[ 2 ], a );
            if ( find( a )->con == "->" ) throw type_error();
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
            tnode *t = infer( it[ 2 ], ctx );
            expect( it[ 3 ], t );
            return t;
        }
        if ( h == "fn" )
        {
            arity( 2 );
            context inner = ctx;
            std::vector< tnode * > ts;
            for ( const auto &p : it[ 1 ].items )
            {
                ts.push_back( fresh() );
                inner.insert_or_assign( p.atom, scheme{ {}, ts.back() } );
            }
            ts.push_back( infer( it[ 2 ], inner ) );
            return mk( "->", ts );
        }
        if ( h == "let" )
        {
            arity( 3 );
            tnode *t = infer( it[ 2 ], ctx );
            context inner = ctx;
            inner.insert_or_assign( it[ 1 ].atom, generalize( ctx, t ) );
            return infer( it[ 3 ], inner );
        }
        if ( h == "letrec" )
        {
            arity( 2 );
            context group = ctx;
            std::vector< tnode * > self;
            for ( const auto &b : it[ 1 ].items )
            {
                if ( b.items.size() != 2 || !b.items[ 1 ].is_list() || b.items[ 1 ].items.empty() ||
                     b.items[ 1 ].items[ 0 ].atom != "fn" )
                    throw type_error();
                self.push_back( fresh() );
                group.insert_or_assign( b.items[ 0 ].atom, scheme{ {}, self.back() } );
            }
            for ( std::size_t i = 0; i < self.size(); ++ i )
                unify( infer( it[ 1 ].items[ i ].items[ 1 ], group ), self[ i ] );
            context inner = ctx;
            for ( std::size_t i = 0; i < self.size(); ++ i )
                inner.insert_or_assign( it[ 1 ].items[ i ].items[ 0 ].atom, generalize( ctx, self[ i ] ) );
            return infer( it[ 2 ], inner );
        }

        tnode *f = infer( it[ 0 ], ctx );
        std::vector< tnode * > ts;
        for ( std::size_t i = 1; i < it.size(); ++ i )
            ts.push_back( infer( it[ i ], ctx ) );
        tnode *r = fresh();
        ts.push_back( r );
        unify( f, mk( "->", ts ) );
        return r;
    }

    long size( tnode *t, std::unordered_map< tnode *, long > &memo )
    {
        t = find( t );
        if ( auto it = memo.find( t ); it != memo.end() ) return it->second;
        long s = 1;
        for ( auto *c : t->args ) s += size( c, memo );
        return memo[ t ] = s;
    }
};

std::optional< long > type_size( std::string_view expr )
{
    try
    {
        dag_inference w;
        tnode *t = w.infer( read( expr ), w.builtins() );
        std::unordered_map< tnode *, long > memo;
        return w.size( t, memo );
    }
    catch ( const type_error & ) { return std::nullopt; }
}

int main()
{
    assert( type_size( "1" ) == 1 );
    assert( type_size( "(fn (x) x)" ) == 3 );
    assert( type_size( "(pair 1 #t)" ) == 3 );
    assert( type_size( "(cons nil nil)" ) == 3 );
    assert( type_size( "(fn (x) (pair x x))" ) == 5 );
    assert( !type_size( "(+ 1 #t)" ) );

    /* f(k) = fn y => f0 (f(k - 1) y); the type of (f(k) 1) has depth k + 1 */
    auto linear = []( int k )
    {
        std::string s = "(let f0 (fn (x) (pair x x)) ";
        for ( int i = 1; i <= k; ++ i )
            s += "(let f" + std::to_string( i ) + " (fn (y) (f0 (f" + std::to_string( i - 1 ) + " y))) ";
        s += "(f" + std::to_string( k ) + " 1)";
        return s + std::string( k + 1, ')' );
    };

    assert( type_size( linear( 0 ) ) == 3 );
    assert( type_size( linear( 3 ) ) == ( 1L << 5 ) - 1 );
    assert( type_size( linear( 40 ) ) == ( 1L << 42 ) - 1 );

    /* doubling: g(k) = fn y => g(k - 1) (g(k - 1) y), depth 2ᵏ */
    std::string doubling = "(let g0 (fn (x) (pair x x)) "
                           "(let g1 (fn (y) (g0 (g0 y))) "
                           "(let g2 (fn (y) (g1 (g1 y))) "
                           "(let g3 (fn (y) (g2 (g2 y))) "
                           "(let g4 (fn (y) (g3 (g3 y))) "
                           "(let g5 (fn (y) (g4 (g4 y))) "
                           "g5))))))";
    assert( type_size( doubling ) == 2 + ( 1L << 33 ) - 1 );

    return 0;
}
