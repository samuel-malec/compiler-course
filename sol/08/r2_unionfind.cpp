#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <map>

/* The substitution in ‹d1_unify› is ‘triangular’: to find what a
 * variable stands for, we may have to follow a long chain of bindings
 * ‹'a ↦ 'b ↦ 'c ↦ …›, and we do that again on every lookup. On large
 * inputs, inference then takes quadratic time. Real implementations
 * use a «union-find» structure instead: each type variable is a node
 * which either is unbound, or points to another node or to a type;
 * lookups follow the pointers and then «compress» the path (make every
 * node on it point directly to the end), and binding a variable to
 * another variable links their «roots» (the smaller tree under the
 * larger one, which keeps the trees shallow).
 *
 * Write a function ‹solve› which takes a list of «equations» between
 * types (each side given as text, in the notation of this chapter)
 * and a list of type variables, solves the equations by unification,
 * and returns, for each of the given variables, the type it is bound
 * to, with all variables resolved as far as possible (written as an
 * S-expression, with spaces between elements; variables which remain
 * unbound keep their original names – when two unbound variables are
 * unified, either of them may remain). Return ‹std::nullopt› if the
 * equations have no solution (including the occurs check).
 *
 * The tests include a chain of a hundred thousand equations, given in
 * an order which makes naive chained substitution take quadratic
 * time. Your solution should be close to linear. The types in the
 * results are small, so you don't need to worry about the time it
 * takes to write them out. You will need to parse the types yourself
 * (there is no ‹read› in this file); they consist of atoms and lists,
 * as usual. */

// std::optional< std::vector< std::string > >
// solve( const std::vector< std::pair< std::string, std::string > > &equations,
//        const std::vector< std::string > &query );

/* Types are parsed into nodes; a variable node is either a root
 * (unbound, ‹parent == -1›) or points to another node. */

struct uf_node
{
    std::string con;            /* atom, or the head of a compound type */
    std::vector< int > args;    /* for compound types */
    bool var = false;
    int parent = -1, rank = 0;
};

struct unifier
{
    std::vector< uf_node > nodes;
    std::map< std::string, int > vars;
    bool failed = false;

    int parse( std::string_view s, std::size_t &pos )
    {
        while ( s[ pos ] == ' ' ) ++ pos;
        if ( s[ pos ] == '(' )
        {
            ++ pos;
            uf_node n;
            bool head = true;
            while ( true )
            {
                while ( s[ pos ] == ' ' ) ++ pos;
                if ( s[ pos ] == ')' ) { ++ pos; break; }
                if ( head )
                {
                    std::size_t start = pos;
                    while ( s[ pos ] != ' ' && s[ pos ] != ')' ) ++ pos;
                    n.con = s.substr( start, pos - start );
                    head = false;
                }
                else
                    n.args.push_back( parse( s, pos ) );
            }
            nodes.push_back( n );
            return nodes.size() - 1;
        }

        std::size_t start = pos;
        while ( pos < s.size() && s[ pos ] != ' ' && s[ pos ] != ')' ) ++ pos;
        std::string a( s.substr( start, pos - start ) );
        if ( a[ 0 ] == '\'' )
        {
            if ( auto it = vars.find( a ); it != vars.end() ) return it->second;
            nodes.push_back( { a, {}, true } );
            return vars[ a ] = nodes.size() - 1;
        }
        nodes.push_back( { a, {}, false } );
        return nodes.size() - 1;
    }

    int parse( std::string_view s ) { std::size_t pos = 0; return parse( s, pos ); }

    int find( int n )
    {
        int root = n;
        while ( nodes[ root ].var && nodes[ root ].parent >= 0 )
            root = nodes[ root ].parent;
        while ( nodes[ n ].var && nodes[ n ].parent >= 0 ) /* path compression */
        {
            int next = nodes[ n ].parent;
            nodes[ n ].parent = root;
            n = next;
        }
        return root;
    }

    void unify( int a0, int b0 )
    {
        std::vector< std::pair< int, int > > work{ { a0, b0 } };
        while ( !work.empty() && !failed )
        {
            auto [ x, y ] = work.back();
            work.pop_back();
            int a = find( x ), b = find( y );
            if ( a == b ) continue;

            uf_node &na = nodes[ a ], &nb = nodes[ b ];
            if ( na.var && nb.var )
            {
                if ( na.rank < nb.rank ) na.parent = b;
                else if ( nb.rank < na.rank ) nb.parent = a;
                else nb.parent = a, ++ na.rank;
            }
            else if ( na.var ) na.parent = b;
            else if ( nb.var ) nb.parent = a;
            else if ( na.con != nb.con || na.args.size() != nb.args.size() ) failed = true;
            else
                for ( std::size_t i = 0; i < na.args.size(); ++ i )
                    work.emplace_back( na.args[ i ], nb.args[ i ] );
        }
    }

    /* The occurs check, done once at the end: the resolved graph must
     * not have a cycle. */
    bool acyclic()
    {
        std::vector< int > state( nodes.size(), 0 ); /* 0 new, 1 active, 2 done */
        std::function< bool( int ) > visit = [ & ]( int n ) -> bool
        {
            n = find( n );
            if ( state[ n ] == 1 ) return false;
            if ( state[ n ] == 2 ) return true;
            state[ n ] = 1;
            for ( int c : nodes[ n ].args )
                if ( !visit( c ) ) return false;
            state[ n ] = 2;
            return true;
        };
        for ( std::size_t i = 0; i < nodes.size(); ++ i )
            if ( !visit( i ) ) return false;
        return true;
    }

    std::string show( int n )
    {
        n = find( n );
        const uf_node &x = nodes[ n ];
        if ( x.args.empty() && x.var ) return x.con;
        std::string out = x.con;
        bool compound = !x.args.empty();
        for ( int c : x.args ) out += " " + show( c );
        return compound ? "(" + out + ")" : out;
    }
};

std::optional< std::vector< std::string > >
solve( const std::vector< std::pair< std::string, std::string > > &equations,
       const std::vector< std::string > &query )
{
    unifier u;
    for ( const auto &[ a, b ] : equations )
    {
        int x = u.parse( a ), y = u.parse( b );
        u.unify( x, y );
        if ( u.failed ) return std::nullopt;
    }
    if ( !u.acyclic() ) return std::nullopt;

    std::vector< std::string > out;
    for ( const auto &q : query )
        out.push_back( u.show( u.parse( q ) ) );
    return out;
}

int main()
{
    using eqs = std::vector< std::pair< std::string, std::string > >;
    using v = std::vector< std::string >;

    assert(( solve( { { "'a", "int" } }, { "'a" } ) == v{ "int" } ));
    assert(( solve( { { "'a", "(-> 'b int)" }, { "'b", "bool" } }, { "'a", "'b" } ) ==
             v{ "(-> bool int)", "bool" } ));
    assert(( solve( { { "(-> 'a 'b)", "(-> 'b (list 'c))" }, { "'c", "int" } }, { "'a" } ) ==
             v{ "(list int)" } ));
    assert(( solve( { { "'a", "'a" } }, { "'a" } ) == v{ "'a" } ));
    assert(( solve( { { "'x", "(pair 'y 'y)" }, { "'y", "(list 'z)" } }, { "'x", "'q" } ) ==
             v{ "(pair (list 'z) (list 'z))", "'q" } ));

    auto r = solve( { { "'a", "'b" } }, { "'a", "'b" } );
    assert( r && ( *r == v{ "'a", "'a" } || *r == v{ "'b", "'b" } ) );

    assert( !solve( { { "int", "bool" } }, {} ) );
    assert( !solve( { { "'a", "(-> 'a int)" } }, {} ) );
    assert( !solve( { { "'a", "(-> 'b int)" }, { "'b", "(list 'a)" } }, {} ) );
    assert( !solve( { { "(pair 'a 'b)", "(-> 'a 'b)" } }, {} ) );

    /* a long chain, linked backwards: 'v1 = 'v0, 'v2 = 'v1, … */
    const int n = 100000;
    eqs chain;
    for ( int i = 1; i <= n; ++ i )
        chain.emplace_back( "'v" + std::to_string( i ), "'v" + std::to_string( i - 1 ) );
    chain.emplace_back( "'v0", "(-> int bool)" );
    v query, expect;
    for ( int i = 0; i <= n; i += 1000 )
        query.push_back( "'v" + std::to_string( i ) ), expect.push_back( "(-> int bool)" );
    assert( solve( chain, query ) == expect );

    return 0;
}
