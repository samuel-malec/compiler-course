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

struct ill_typed {};

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
bool is( const type &t, std::string_view h ) { return t.is_list() && !t.items.empty() && t.items[ 0 ].atom == h; }

std::set< std::string > free_tvars( const type &t )
{
    if ( t.is_atom() )
        return t.atom == "int" || t.atom == "bool" ? std::set< std::string >{} : std::set< std::string >{ t.atom };
    std::set< std::string > out;
    if ( is( t, "forall" ) )
    {
        out = free_tvars( t.items[ 2 ] );
        for ( const auto &v : t.items[ 1 ].items ) out.erase( v.atom );
        return out;
    }
    for ( std::size_t i = 1; i < t.items.size(); ++ i )
        out.merge( free_tvars( t.items[ i ] ) );
    return out;
}

using tsubst = std::map< std::string, type >;

type subst( const type &t, tsubst s )
{
    if ( t.is_atom() )
        return s.contains( t.atom ) ? s.at( t.atom ) : t;
    if ( !is( t, "forall" ) )
    {
        type out = t;
        for ( std::size_t i = 1; i < out.items.size(); ++ i )
            out.items[ i ] = subst( t.items[ i ], s );
        return out;
    }

    type vars = t.items[ 1 ];
    type body = t.items[ 2 ];
    for ( const auto &v : vars.items ) s.erase( v.atom );   /* shadowed */

    auto body_fv = free_tvars( body );
    std::set< std::string > repl_fv;
    bool relevant = false;
    for ( const auto &[ name, ty ] : s )
        if ( body_fv.contains( name ) )
            relevant = true, repl_fv.merge( free_tvars( ty ) );

    if ( relevant )
        for ( auto &v : vars.items )
            if ( repl_fv.contains( v.atom ) )
            {
                std::string fresh = v.atom + "'";
                while ( repl_fv.contains( fresh ) || body_fv.contains( fresh ) )
                    fresh += "'";
                body = subst( body, { { v.atom, atom( fresh ) } } );
                body_fv = free_tvars( body );
                v.atom = fresh;
            }

    return sexp{ "", { t.items[ 0 ], vars, subst( body, s ) }, 0 };
}

/* α-equivalence: compare, mapping bound variables to their binding
 * positions (as in ‹03/r1_alpha›). */

bool alpha( const type &a, const type &b, std::vector< std::pair< std::string, std::string > > &bound )
{
    if ( a.is_atom() && b.is_atom() )
    {
        for ( auto it = bound.rbegin(); it != bound.rend(); ++ it )
            if ( it->first == a.atom || it->second == b.atom )
                return it->first == a.atom && it->second == b.atom;
        return a.atom == b.atom;
    }
    if ( a.is_atom() || b.is_atom() || a.items.size() != b.items.size() || a.items[ 0 ].atom != b.items[ 0 ].atom )
        return false;

    if ( is( a, "forall" ) )
    {
        if ( a.items[ 1 ].items.size() != b.items[ 1 ].items.size() ) return false;
        auto mark = bound.size();
        for ( std::size_t i = 0; i < a.items[ 1 ].items.size(); ++ i )
            bound.emplace_back( a.items[ 1 ].items[ i ].atom, b.items[ 1 ].items[ i ].atom );
        bool r = alpha( a.items[ 2 ], b.items[ 2 ], bound );
        bound.resize( mark );
        return r;
    }

    for ( std::size_t i = 1; i < a.items.size(); ++ i )
        if ( !alpha( a.items[ i ], b.items[ i ], bound ) )
            return false;
    return true;
}

bool same( const type &a, const type &b )
{
    std::vector< std::pair< std::string, std::string > > bound;
    return alpha( a, b, bound );
}

struct checker
{
    std::vector< std::string > tvars = {};
    std::map< std::string, type > ctx = {};

    bool tvar_in_scope( const std::string &v ) const
    {
        for ( const auto &x : tvars ) if ( x == v ) return true;
        return false;
    }

    void valid( const type &t, std::vector< std::string > &extra )
    {
        if ( t.is_atom() )
        {
            if ( t.atom == "int" || t.atom == "bool" || tvar_in_scope( t.atom ) ) return;
            for ( const auto &x : extra ) if ( x == t.atom ) return;
            throw ill_typed();
        }
        if ( is( t, "forall" ) )
        {
            if ( t.items.size() != 3 || !t.items[ 1 ].is_list() || t.items[ 1 ].items.empty() ) throw ill_typed();
            auto mark = extra.size();
            std::set< std::string > seen;
            for ( const auto &v : t.items[ 1 ].items )
            {
                if ( !v.is_atom() || !seen.insert( v.atom ).second ) throw ill_typed();
                extra.push_back( v.atom );
            }
            valid( t.items[ 2 ], extra );
            extra.resize( mark );
            return;
        }
        if ( !is( t, "->" ) || t.items.size() < 2 ) throw ill_typed();
        for ( std::size_t i = 1; i < t.items.size(); ++ i ) valid( t.items[ i ], extra );
    }

    void valid( const type &t ) { std::vector< std::string > extra; valid( t, extra ); }

    void expect( const sexp &e, const type &t ) { if ( !same( check( e ), t ) ) throw ill_typed(); }

    type check( const sexp &e )
    {
        const type int_t = atom( "int" ), bool_t = atom( "bool" );

        if ( e.is_atom() )
        {
            if ( e.atom == "#t" || e.atom == "#f" ) return bool_t;
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos ) return int_t;
            if ( auto it = ctx.find( e.atom ); it != ctx.end() ) return it->second;
            throw ill_typed();
        }

        const auto &it = e.items;
        if ( it.empty() ) throw ill_typed();
        const std::string &h = it[ 0 ].atom;
        auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw ill_typed(); };

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
            type t = check( it[ 1 ] );
            if ( !same( t, int_t ) && !same( t, bool_t ) ) throw ill_typed();
            expect( it[ 2 ], t );
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
            type t = check( it[ 2 ] );
            expect( it[ 3 ], t );
            return t;
        }
        if ( h == "let" )
        {
            arity( 3 );
            type v = check( it[ 2 ] );
            auto saved = ctx;
            ctx.insert_or_assign( it[ 1 ].atom, v );
            type r = check( it[ 3 ] );
            ctx = saved;
            return r;
        }
        if ( h == "fn" )
        {
            arity( 2 );
            auto saved = ctx;
            type out{ "", { atom( "->" ) }, 0 };
            std::set< std::string > seen;
            for ( const auto &p : it[ 1 ].items )
            {
                if ( !p.is_list() || p.items.size() != 2 || !seen.insert( p.items[ 0 ].atom ).second )
                    throw ill_typed();
                valid( p.items[ 1 ] );
                ctx.insert_or_assign( p.items[ 0 ].atom, p.items[ 1 ] );
                out.items.push_back( p.items[ 1 ] );
            }
            out.items.push_back( check( it[ 2 ] ) );
            ctx = saved;
            return out;
        }
        if ( h == "tfn" )
        {
            arity( 2 );
            if ( !it[ 1 ].is_list() || it[ 1 ].items.empty() ) throw ill_typed();
            auto mark = tvars.size();
            std::set< std::string > seen;
            for ( const auto &v : it[ 1 ].items )
            {
                if ( !v.is_atom() || !seen.insert( v.atom ).second ) throw ill_typed();
                tvars.push_back( v.atom );
            }
            type body = check( it[ 2 ] );
            tvars.resize( mark );
            return sexp{ "", { atom( "forall" ), it[ 1 ], body }, 0 };
        }
        if ( h == "inst" )
        {
            if ( it.size() < 3 ) throw ill_typed();
            type t = check( it[ 1 ] );
            if ( !is( t, "forall" ) || t.items[ 1 ].items.size() != it.size() - 2 ) throw ill_typed();
            tsubst s;
            for ( std::size_t i = 2; i < it.size(); ++ i )
            {
                valid( it[ i ] );
                s.emplace( t.items[ 1 ].items[ i - 2 ].atom, it[ i ] );
            }
            return subst( t.items[ 2 ], s );
        }

        type f = check( it[ 0 ] );
        if ( !is( f, "->" ) || f.items.size() != it.size() + 1 ) throw ill_typed();
        for ( std::size_t i = 1; i < it.size(); ++ i ) expect( it[ i ], f.items[ i ] );
        return f.items.back();
    }
};

std::optional< std::string > type_of( std::string_view expr )
{
    try { checker c; return write( c.check( read( expr ) ) ); }
    catch ( const ill_typed & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

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
