#include <cassert>
#include <optional>
#include <string_view>
#include <map>
#include <string>
#include <vector>

/* Write a function ‹tautology› which parses a formula of
 * propositional logic and decides whether it is a «tautology» (true
 * under every assignment of truth values to its variables). The
 * syntax, from the loosest to the tightest binding:
 *
 *  • ‹a <-> b› – equivalence, left-associative,
 *  • ‹a -> b›  – implication, «right»-associative (‹a -> b -> c› is
 *    ‹a -> (b -> c)›),
 *  • ‹a | b›   – disjunction, left-associative,
 *  • ‹a & b›   – conjunction, left-associative,
 *  • ‹!a›      – negation (prefix),
 *  • atoms: variables (non-empty sequences of lowercase letters), the
 *    constants ‹1› (true) and ‹0› (false), and parenthesised formulas.
 *
 * Spaces may appear between tokens. If the input is not a valid
 * formula, return ‹std::nullopt›.
 *
 * Consider first parsing the formula into a tree (a vector of nodes
 * which refer to each other by index is a simple representation),
 * and then evaluating the tree for each assignment. You can assume
 * there are at most 16 distinct variables. */

// std::optional< bool > tautology( std::string_view formula );

/* The tree is stored in a vector; children are referred to by
 * index. Variables are numbered in order of appearance. */

struct fnode
{
    char op;          /* 'v' variable, 'c' constant, '!', '&', '|', '>', '=' */
    int a = -1, b = -1;
};

struct taut_parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;
    std::vector< fnode > nodes = {};
    std::map< std::string, int > vars = {};

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    bool accept( std::string_view t )
    {
        if ( peek() != t ) return false;
        ++ pos;
        return true;
    }

    int add( fnode n ) { nodes.push_back( n ); return nodes.size() - 1; }

    /* Each level returns the index of its node, or -1 on error. */

    int equiv()
    {
        int l = impl();
        while ( l >= 0 && accept( "<->" ) )
        {
            int r = impl();
            l = r < 0 ? -1 : add( { '=', l, r } );
        }
        return l;
    }

    int impl()
    {
        int l = disj();
        if ( l < 0 || !accept( "->" ) )
            return l;
        int r = impl(); /* right recursion = right associativity */
        return r < 0 ? -1 : add( { '>', l, r } );
    }

    int disj()
    {
        int l = conj();
        while ( l >= 0 && accept( "|" ) )
        {
            int r = conj();
            l = r < 0 ? -1 : add( { '|', l, r } );
        }
        return l;
    }

    int conj()
    {
        int l = neg();
        while ( l >= 0 && accept( "&" ) )
        {
            int r = neg();
            l = r < 0 ? -1 : add( { '&', l, r } );
        }
        return l;
    }

    int neg()
    {
        if ( accept( "!" ) )
        {
            int a = neg();
            return a < 0 ? -1 : add( { '!', a } );
        }
        return atom();
    }

    int atom()
    {
        if ( accept( "(" ) )
        {
            int e = equiv();
            return e >= 0 && accept( ")" ) ? e : -1;
        }

        std::string t = peek();
        if ( t == "0" || t == "1" )
            return ++ pos, add( { 'c', t == "1" } );
        if ( !t.empty() && t[ 0 ] >= 'a' && t[ 0 ] <= 'z' )
        {
            ++ pos;
            auto [ it, _ ] = vars.emplace( t, vars.size() );
            return add( { 'v', it->second } );
        }
        return -1;
    }
};

bool eval( const std::vector< fnode > &nodes, int n, unsigned assignment )
{
    const fnode &f = nodes[ n ];
    auto a = [ & ] { return eval( nodes, f.a, assignment ); };
    auto b = [ & ] { return eval( nodes, f.b, assignment ); };

    switch ( f.op )
    {
        case 'v': return assignment >> f.a & 1;
        case 'c': return f.a;
        case '!': return !a();
        case '&': return a() && b();
        case '|': return a() || b();
        case '>': return !a() || b();
        default:  return a() == b();
    }
}

std::optional< bool > tautology( std::string_view in )
{
    std::vector< std::string > toks;

    for ( std::size_t i = 0; i < in.size(); )
    {
        std::size_t start = i++;
        char c = in[ start ];

        if ( c == ' ' )
            continue;
        if ( c >= 'a' && c <= 'z' )
            while ( i < in.size() && in[ i ] >= 'a' && in[ i ] <= 'z' )
                ++ i;
        else if ( in.substr( start ).starts_with( "<->" ) )
            i = start + 3;
        else if ( in.substr( start ).starts_with( "->" ) )
            i = start + 2;
        else if ( std::string_view( "01!&|()" ).find( c ) == in.npos )
            return std::nullopt;

        toks.emplace_back( in.substr( start, i - start ) );
    }

    taut_parser p{ toks };
    int root = p.equiv();
    if ( root < 0 || p.pos != toks.size() )
        return std::nullopt;

    for ( unsigned a = 0; a < 1u << p.vars.size(); ++ a )
        if ( !eval( p.nodes, root, a ) )
            return false;

    return true;
}

int main()
{
    assert( tautology( "1" ) == true );
    assert( tautology( "0" ) == false );
    assert( tautology( "p" ) == false );
    assert( tautology( "p | !p" ) == true );
    assert( tautology( "p & !p" ) == false );
    assert( tautology( "p -> p" ) == true );
    assert( tautology( "p -> q" ) == false );
    assert( tautology( "(p -> q) -> (!q -> !p)" ) == true );
    assert( tautology( "p -> q -> p" ) == true );        /* right assoc */
    assert( tautology( "(p -> q) -> p" ) == false );
    assert( tautology( "((p -> q) -> p) -> p" ) == true ); /* Peirce */
    assert( tautology( "p & q -> p" ) == true );
    assert( tautology( "p | q & r <-> (p | q) & (p | r)" ) == true );
    assert( tautology( "!(a & b) <-> !a | !b" ) == true );
    assert( tautology( "a <-> b" ) == false );
    assert( tautology( "p & !p -> anything" ) == true );
    assert( tautology( "!!x <-> x" ) == true );
    assert( tautology( "x <-> y <-> (x <-> y)" ) == true );

    assert( !tautology( "" ) );
    assert( !tautology( "p &" ) );
    assert( !tautology( "(p" ) );
    assert( !tautology( "p q" ) );
    assert( !tautology( "p => q" ) );
    assert( !tautology( "p - > q" ) );
    assert( !tautology( "P" ) );

    return 0;
}
