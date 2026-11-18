#include <cassert>
#include <expected>
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

/* A value constructor: its data type (with the declaration's parameters)
 * and its field types. */
struct constructor
{
    std::string data;
    std::vector< std::string > params;
    std::vector< sexp > fields;
    sexp result;
};

std::map< std::string, constructor > constructors;
std::map< std::string, std::vector< std::string > > data_cons; /* data type → its constructors */

bool is_con( const std::string &a ) { return !a.empty() && a[ 0 ] >= 'A' && a[ 0 ] <= 'Z'; }

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

    std::vector< const sexp * > matches = {};

    /* Instantiate a constructor: fresh variables for the parameters;
     * returns the result type and fills in the field types. */
    type con_type( const constructor &c, std::vector< type > &fields )
    {
        std::map< std::string, type > m;
        for ( const auto &p : c.params ) m.emplace( p, fresh() );
        std::function< type( const type & ) > copy = [ & ]( const type &t ) -> type
        {
            if ( is_var( t ) ) return m.at( t.atom );
            type out = t;
            for ( auto &x : out.items ) x = copy( x );
            return out;
        };
        for ( const auto &f : c.fields ) fields.push_back( copy( f ) );
        return copy( c.result );
    }

    type construct( const sexp &e, const context &ctx )
    {
        const std::string &name = e.is_atom() ? e.atom : e.items[ 0 ].atom;
        auto c = constructors.find( name );
        if ( c == constructors.end() ) bad( e );
        std::vector< type > fields;
        type r = con_type( c->second, fields );
        std::size_t given = e.is_atom() ? 0 : e.items.size() - 1;
        if ( given != fields.size() || ( !e.is_atom() && fields.empty() ) ) bad( e );
        for ( std::size_t i = 0; i < given; ++ i )
            unify( infer( e.items[ i + 1 ], ctx ), fields[ i ], e.items[ i + 1 ].offset );
        return r;
    }

    void pattern( const sexp &p, const type &t, context &ctx, std::set< std::string > &seen )
    {
        if ( p.is_atom() )
        {
            if ( p.atom == "_" ) return;
            if ( p.atom == "#t" || p.atom == "#f" ) return unify( t, atom( "bool" ), p.offset );
            if ( p.atom.find_first_not_of( "-0123456789" ) == p.atom.npos ) return unify( t, atom( "int" ), p.offset );
            if ( !is_con( p.atom ) )
            {
                if ( !seen.insert( p.atom ).second ) bad( p );
                ctx.insert_or_assign( p.atom, mono( t ) );
                return;
            }
        }

        const std::string &name = p.is_atom() ? p.atom : p.items.at( 0 ).atom;
        auto c = constructors.find( name );
        if ( c == constructors.end() ) bad( p );
        std::vector< type > fields;
        unify( t, con_type( c->second, fields ), p.offset );
        std::size_t given = p.is_atom() ? 0 : p.items.size() - 1;
        if ( given != fields.size() || ( !p.is_atom() && fields.empty() ) ) bad( p );
        for ( std::size_t i = 0; i < given; ++ i )
            pattern( p.items[ i + 1 ], fields[ i ], ctx, seen );
    }


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
            if ( is_con( e.atom ) ) return construct( e, ctx );
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
        if ( is_con( h ) )
            return construct( e, ctx );
        if ( h == "match" )
        {
            if ( it.size() < 3 ) bad( e );
            type scrutinee = infer( it[ 1 ], ctx );
            type result = fresh();
            for ( std::size_t i = 2; i < it.size(); ++ i )
            {
                const sexp &c = it[ i ];
                if ( !c.is_list() || c.items.size() != 2 ) bad( c );
                context inner = ctx;
                std::set< std::string > seen;
                pattern( c.items[ 0 ], scrutinee, inner, seen );
                unify( infer( c.items[ 1 ], inner ), result, c.offset );
            }
            matches.push_back( &e );
            return result;
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
                unify( infer( it[ 1 ].items[ i ].items[ 1 ], group ), self[ i ], 0 );
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

/* ─── exhaustiveness and redundancy (Maranget's usefulness) ─────── */

/* A pattern, simplified: a wildcard (variables count as wildcards) or
 * a constructor – a value constructor, ‹#t›, ‹#f› or an integer literal –
 * with sub-patterns. */

using row = std::vector< const sexp * >;

bool is_wild( const sexp *p ) { return p->is_atom() && !is_con( p->atom ) &&
                                       p->atom != "#t" && p->atom != "#f" &&
                                       p->atom.find_first_not_of( "-0123456789" ) != p->atom.npos; }

std::string head( const sexp *p ) { return p->is_atom() ? p->atom : p->items[ 0 ].atom; }

std::size_t arity( const std::string &c )
{
    auto it = constructors.find( c );
    return it == constructors.end() ? 0 : it->second.fields.size();
}

const sexp wildcard{ "_", {}, 0 };

/* specialize a row by constructor ‹c› (or ‹std::nullopt› if it is dropped) */
std::optional< row > specialize( const row &r, const std::string &c, std::size_t n )
{
    row out;
    if ( is_wild( r[ 0 ] ) )
        out.assign( n, &wildcard );
    else if ( head( r[ 0 ] ) == c )
        for ( std::size_t i = 1; i <= n; ++ i ) out.push_back( &r[ 0 ]->items[ i ] );
    else
        return std::nullopt;
    out.insert( out.end(), r.begin() + 1, r.end() );
    return out;
}

bool useful( const std::vector< row > &P, const row &q )
{
    if ( q.empty() )
        return P.empty();

    auto rec = [ & ]( const std::string &c, std::size_t n, const row &qq )
    {
        std::vector< row > S;
        for ( const auto &r : P )
            if ( auto s = specialize( r, c, n ) ) S.push_back( *s );
        auto sq = specialize( qq, c, n );
        return useful( S, *sq );
    };

    if ( !is_wild( q[ 0 ] ) )
        return rec( head( q[ 0 ] ), arity( head( q[ 0 ] ) ), q );

    /* q starts with a wildcard: is the signature of the first column complete? */
    std::set< std::string > sigma;
    for ( const auto &r : P )
        if ( !is_wild( r[ 0 ] ) ) sigma.insert( head( r[ 0 ] ) );

    std::vector< std::string > all;
    bool complete = false;
    if ( !sigma.empty() )
    {
        const std::string &c = *sigma.begin();
        if ( c == "#t" || c == "#f" ) all = { "#f", "#t" };
        else if ( is_con( c ) ) all = data_cons.at( constructors.at( c ).data );
        complete = !all.empty();
        for ( const auto &x : all ) complete = complete && sigma.contains( x );
    }

    if ( complete )
    {
        for ( const auto &c : all )
            if ( rec( c, arity( c ), q ) ) return true;
        return false;
    }

    /* the default matrix: rows starting with a wildcard */
    std::vector< row > D;
    for ( const auto &r : P )
        if ( is_wild( r[ 0 ] ) ) D.emplace_back( r.begin() + 1, r.end() );
    return useful( D, row( q.begin() + 1, q.end() ) );
}

/* 0 = fine, 1 = redundant, 2 = non-exhaustive */
int check_match( const sexp &m )
{
    std::vector< row > P;
    for ( std::size_t i = 2; i < m.items.size(); ++ i )
    {
        row r{ &m.items[ i ].items[ 0 ] };
        if ( !useful( P, r ) ) return 1;
        P.push_back( r );
    }
    return useful( P, row{ &wildcard } ) ? 2 : 0;
}

/* ─── declarations ───────────────────────────────────────────────── */

struct bad_data {};

void check_type( const sexp &t, const std::set< std::string > &params,
                 const std::map< std::string, std::size_t > &arities )
{
    if ( t.is_atom() )
    {
        if ( t.atom == "int" || t.atom == "bool" ) return;
        if ( t.atom[ 0 ] == '\'' ) { if ( !params.contains( t.atom ) ) throw bad_data(); return; }
        auto a = arities.find( t.atom );
        if ( a == arities.end() || a->second != 0 ) throw bad_data();
        return;
    }
    if ( t.items.empty() || !t.items[ 0 ].is_atom() ) throw bad_data();
    const std::string &h = t.items[ 0 ].atom;
    if ( h == "->" ) { if ( t.items.size() < 2 ) throw bad_data(); }
    else
    {
        auto a = arities.find( h );
        if ( a == arities.end() || a->second == 0 || a->second != t.items.size() - 1 ) throw bad_data();
    }
    for ( std::size_t i = 1; i < t.items.size(); ++ i ) check_type( t.items[ i ], params, arities );
}

std::expected< std::vector< std::string >, std::string > infer( std::string_view program )
{
    constructors.clear();
    data_cons.clear();

    sexp p;
    try { p = read( program ); }
    catch ( const std::invalid_argument & ) { return std::unexpected( "syntax error" ); }

    if ( !p.is_list() || p.items.empty() || p.items[ 0 ].atom != "program" )
        return std::unexpected( "syntax error" );
    for ( std::size_t i = 1; i < p.items.size(); ++ i )
    {
        const sexp &item = p.items[ i ];
        if ( !item.is_list() || item.items.size() < 2 ||
             ( item.items[ 0 ].atom != "data" && item.items[ 0 ].atom != "define" ) ||
             ( item.items[ 0 ].atom == "define" && ( item.items.size() != 3 || !item.items[ 1 ].is_atom() ) ) )
            return std::unexpected( "syntax error" );
    }

    /* data: first collect the type names and arities, then check */
    std::map< std::string, std::size_t > arities;
    std::vector< const sexp * > datas;
    for ( std::size_t i = 1; i < p.items.size(); ++ i )
        if ( p.items[ i ].items[ 0 ].atom == "data" )
            datas.push_back( &p.items[ i ] );

    auto data_name = []( const sexp &d ) -> std::string
    {
        const sexp &h = d.items[ 1 ];
        return h.is_atom() ? h.atom : h.items.empty() ? "" : h.items[ 0 ].atom;
    };

    for ( auto d : datas )
    {
        std::string name = data_name( *d );
        try
        {
            const sexp &h = d->items[ 1 ];
            if ( name.empty() || !( name[ 0 ] >= 'a' && name[ 0 ] <= 'z' ) || name == "int" || name == "bool" ||
                 arities.contains( name ) || d->items.size() < 3 || ( h.is_list() && h.items.size() < 2 ) )
                throw bad_data();
            arities[ name ] = h.is_atom() ? 0 : h.items.size() - 1;
        }
        catch ( const bad_data & ) { return std::unexpected( "bad data " + name ); }
    }

    for ( auto d : datas )
    {
        std::string name = data_name( *d );
        try
        {
            const sexp &h = d->items[ 1 ];
            std::vector< std::string > params;
            std::set< std::string > pset;
            if ( h.is_list() )
                for ( std::size_t i = 1; i < h.items.size(); ++ i )
                {
                    if ( !h.items[ i ].is_atom() || h.items[ i ].atom[ 0 ] != '\'' ||
                         !pset.insert( h.items[ i ].atom ).second )
                        throw bad_data();
                    params.push_back( h.items[ i ].atom );
                }

            sexp result = h.is_atom() ? h : h;
            for ( std::size_t i = 2; i < d->items.size(); ++ i )
            {
                const sexp &c = d->items[ i ];
                if ( !c.is_list() || c.items.empty() || !c.items[ 0 ].is_atom() || !is_con( c.items[ 0 ].atom ) ||
                     constructors.contains( c.items[ 0 ].atom ) )
                    throw bad_data();
                constructor k{ name, params, {}, result };
                for ( std::size_t j = 1; j < c.items.size(); ++ j )
                {
                    check_type( c.items[ j ], pset, arities );
                    k.fields.push_back( c.items[ j ] );
                }
                constructors.emplace( c.items[ 0 ].atom, k );
                data_cons[ name ].push_back( c.items[ 0 ].atom );
            }
        }
        catch ( const bad_data & ) { return std::unexpected( "bad data " + name ); }
    }

    /* definitions */
    inference w;
    context ctx;
    std::set< std::string > defined;
    std::vector< std::string > out;

    for ( std::size_t i = 1; i < p.items.size(); ++ i )
    {
        const sexp &item = p.items[ i ];
        if ( item.items[ 0 ].atom != "define" ) continue;
        const std::string &name = item.items[ 1 ].atom;
        if ( !defined.insert( name ).second )
            return std::unexpected( "duplicate define " + name );

        w.matches.clear();
        type self;
        try
        {
            self = w.fresh();
            context rec = ctx;
            rec.insert_or_assign( name, inference::mono( self ) );
            w.unify( w.infer( item.items[ 2 ], rec ), self, 0 );
        }
        catch ( const type_error & ) { return std::unexpected( "type error in " + name ); }

        /* pre-order: a match is recorded after its sub-matches, so sort
         * the matches by position */
        std::sort( w.matches.begin(), w.matches.end(),
                   []( auto a, auto b ) { return a->offset < b->offset; } );
        for ( auto m : w.matches )
            if ( int r = check_match( *m ) )
                return std::unexpected( ( r == 1 ? "redundant pattern in " : "non-exhaustive match in " ) + name );

        ctx.insert_or_assign( name, w.generalize( ctx, self ) );
        std::map< std::string, std::string > names;
        out.push_back( name + " : " + write( normalize( w.apply( self ), names ) ) );
    }

    return out;
}

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
