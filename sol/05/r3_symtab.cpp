#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

/* Every compiler needs a «symbol table»: a structure which maps names
 * to information about them (here, just an ‹int›), and which follows
 * the nesting of scopes. A stack of maps (as in the demos) is simple,
 * but a lookup has to search through all the scopes, so a name
 * declared in the global scope takes ⟦O(d)⟧ steps to find, where ⟦d⟧
 * is the nesting depth.
 *
 * Write a class ‹symtab› where «all» operations take (amortized,
 * expected) constant time, not counting the length of the names – in
 * particular, independent of the number of scopes:
 *
 *  • ‹enter()› – open a new scope,
 *  • ‹leave()› – close the innermost scope, removing everything that
 *    was declared in it (this may take time proportional to the number
 *    of names declared in that scope); throws ‹std::logic_error› if
 *    only the global scope is open,
 *  • ‹declare( name, value )› – declare ‹name› in the innermost scope;
 *    returns ‹false› (and does nothing) if the innermost scope already
 *    declares ‹name›,
 *  • ‹lookup( name )› – the value of the innermost visible declaration
 *    of ‹name›, or ‹std::nullopt›,
 *  • ‹depth()› – the number of scopes opened by ‹enter› which have not
 *    been closed yet (0 for just the global scope).
 *
 * Hint: map each name to a «stack» of its declarations (each
 * remembering the scope it belongs to), and keep, for each open scope,
 * the list of names declared in it. */

// class symtab;

class symtab
{
    struct decl { std::size_t scope; int value; };

    std::unordered_map< std::string, std::vector< decl > > _names;
    std::vector< std::vector< std::string > > _scopes{ {} };

public:
    void enter() { _scopes.emplace_back(); }

    void leave()
    {
        if ( _scopes.size() == 1 )
            throw std::logic_error( "cannot leave the global scope" );
        for ( const auto &n : _scopes.back() )
        {
            auto it = _names.find( n );
            it->second.pop_back();
            if ( it->second.empty() )
                _names.erase( it );
        }
        _scopes.pop_back();
    }

    bool declare( const std::string &name, int value )
    {
        auto &stack = _names[ name ];
        std::size_t current = _scopes.size() - 1;
        if ( !stack.empty() && stack.back().scope == current )
            return false;
        stack.push_back( { current, value } );
        _scopes.back().push_back( name );
        return true;
    }

    std::optional< int > lookup( const std::string &name ) const
    {
        auto it = _names.find( name );
        if ( it == _names.end() || it->second.empty() )
            return std::nullopt;
        return it->second.back().value;
    }

    std::size_t depth() const { return _scopes.size() - 1; }
};

int main()
{
    symtab t;
    assert( t.depth() == 0 );
    assert( t.declare( "x", 1 ) );
    assert( !t.declare( "x", 2 ) );
    assert( t.lookup( "x" ) == 1 );

    t.enter();
    assert( t.depth() == 1 );
    assert( t.lookup( "x" ) == 1 );
    assert( t.declare( "x", 10 ) );
    assert( t.declare( "y", 20 ) );
    assert( t.lookup( "x" ) == 10 );
    t.enter();
    assert( t.lookup( "y" ) == 20 );
    t.leave();
    t.leave();
    assert( t.lookup( "x" ) == 1 );
    assert( !t.lookup( "y" ) );
    assert( t.declare( "y", 3 ) );

    bool threw = false;
    try { t.leave(); } catch ( const std::logic_error & ) { threw = true; }
    assert( threw );

    /* deep nesting: lookups of global names must not walk the scopes */
    symtab big;
    const int n = 20000;
    for ( int i = 0; i < 1000; ++ i )
        big.declare( "g" + std::to_string( i ), i );
    for ( int i = 0; i < n; ++ i )
    {
        big.enter();
        big.declare( "l" + std::to_string( i ), i );
    }
    long sum = 0;
    for ( int k = 0; k < 20; ++ k )
        for ( int i = 0; i < 1000; ++ i )
            sum += *big.lookup( "g" + std::to_string( i ) );
    assert( sum == 20 * 999 * 1000 / 2 );
    assert( big.depth() == n );
    assert( big.lookup( "l0" ) == 0 );
    for ( int i = 0; i < n; ++ i )
        big.leave();
    assert( !big.lookup( "l0" ) );
    assert( big.lookup( "g7" ) == 7 );

    return 0;
}
