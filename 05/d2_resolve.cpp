#include <cassert>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

/* The interpreter in ‹d1_stmt› looks variables up «by name», every
 * time they are used, searching through all the scopes. That is slow,
 * and it means that errors like an undeclared variable are only found
 * when (and if) the offending statement runs. In this demo, we split
 * the work into two passes:
 *
 *  1. The «resolver» walks the program once, before it runs, keeping a
 *     stack of scopes, each a list of the names declared in it so far.
 *     For each use of a variable, it records a pair (depth, slot):
 *     how many scopes up the variable lives, and its position in that
 *     scope. Errors are reported here – for the whole program.
 *  2. The interpreter keeps a stack of «frames», each a plain vector of
 *     values, one per variable declared in the corresponding scope.
 *     A variable access is just ‹frames[ top - depth ][ slot ]›.
 *
 * This is exactly what a compiler does, except that a compiler goes
 * one step further and turns the pairs into memory addresses.
 *
 * The program uses the S-expression syntax described in the
 * introduction (without ‹if› and ‹while›, to keep the demo short):
 *
 *     stmt = (block stmt …) | (var name expr) | (set name expr)
 *          | (print expr) ;
 *     expr = integer | name | (+ expr expr) | (* expr expr) ;     */

struct resolve_error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/* The result of resolution: for every ‹sexp› node which names a
 * variable (an atom in an expression, or the target of ‹set›), the
 * location of the variable. We use the address of the node as the
 * key – the tree does not change after reading, so the addresses are
 * stable. */

using location = std::pair< int, int >; /* depth, slot */
using resolution = std::map< const sexp *, location >;

bool is_number( const sexp &e )
{
    return e.is_atom() && e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos;
}

struct resolver
{
    std::vector< std::vector< std::string > > scopes;
    resolution out = {};

    location lookup( const sexp &name )
    {
        /* Search the scopes from the innermost; the depth is the number
         * of scopes we had to skip. Within a scope, search from the
         * back is not needed: names are unique in a scope. */
        for ( int d = 0; d < int( scopes.size() ); ++ d )
        {
            const auto &scope = scopes[ scopes.size() - 1 - d ];
            for ( int slot = 0; slot < int( scope.size() ); ++ slot )
                if ( scope[ slot ] == name.atom )
                    return { d, slot };
        }
        throw resolve_error( "undeclared variable " + name.atom );
    }

    void expr( const sexp &e )
    {
        if ( is_number( e ) )
            return;
        if ( e.is_atom() )
            out[ &e ] = lookup( e );
        else
            expr( e.items[ 1 ] ), expr( e.items[ 2 ] );
    }

    void stmt( const sexp &s )
    {
        const std::string &head = s.items[ 0 ].atom;

        if ( head == "block" )
        {
            scopes.emplace_back();
            for ( std::size_t i = 1; i < s.items.size(); ++ i )
                stmt( s.items[ i ] );
            scopes.pop_back();
        }
        else if ( head == "var" )
        {
            /* Resolve the initializer «before» declaring the name: in
             * ‹(var x x)›, the second ‹x› refers to an outer variable. */
            expr( s.items[ 2 ] );
            auto &scope = scopes.back();
            for ( const auto &n : scope )
                if ( n == s.items[ 1 ].atom )
                    throw resolve_error( "redeclared variable " + n );
            scope.push_back( s.items[ 1 ].atom );
        }
        else if ( head == "set" )
        {
            out[ &s.items[ 1 ] ] = lookup( s.items[ 1 ] );
            expr( s.items[ 2 ] );
        }
        else
            expr( s.items[ 1 ] ); /* print */
    }
};

/* The interpreter. Frames grow as variables are declared, so the slot
 * of a new variable is always the current size of the top frame –
 * the same numbering the resolver used. */

struct interpreter
{
    const resolution &res;
    std::vector< std::vector< long > > frames = {};
    std::vector< long > output = {};

    long &var( const sexp &name )
    {
        auto [ depth, slot ] = res.at( &name );
        return frames[ frames.size() - 1 - depth ][ slot ];
    }

    long eval( const sexp &e )
    {
        if ( is_number( e ) )
            return std::stol( e.atom );
        if ( e.is_atom() )
            return var( e );
        long a = eval( e.items[ 1 ] ), b = eval( e.items[ 2 ] );
        return e.items[ 0 ].atom == "+" ? a + b : a * b;
    }

    void exec( const sexp &s )
    {
        const std::string &head = s.items[ 0 ].atom;

        if ( head == "block" )
        {
            frames.emplace_back();
            for ( std::size_t i = 1; i < s.items.size(); ++ i )
                exec( s.items[ i ] );
            frames.pop_back();
        }
        else if ( head == "var" )
        {
            long v = eval( s.items[ 2 ] );
            frames.back().push_back( v );
        }
        else if ( head == "set" )
            var( s.items[ 1 ] ) = eval( s.items[ 2 ] );
        else
            output.push_back( eval( s.items[ 1 ] ) );
    }
};

std::vector< long > run( std::string_view src )
{
    sexp program = read( src );
    resolver r;
    r.stmt( program );
    interpreter i{ r.out };
    i.exec( program );
    return i.output;
}

int main() /* demo */
{
    using v = std::vector< long >;

    sexp p = read( "(block (var x 1) (block (var y 2) (print (+ x y))))" );
    resolver r;
    r.stmt( p );

    /* ‹x› in the inner block is 1 scope up, slot 0; ‹y› is local, slot 0 */
    const sexp &sum = p.items[ 2 ].items[ 2 ].items[ 1 ];
    assert(( r.out.at( &sum.items[ 1 ] ) == location{ 1, 0 } ));
    assert(( r.out.at( &sum.items[ 2 ] ) == location{ 0, 0 } ));

    assert(( run( "(block (var x 1) (block (var y 2) (print (+ x y))))" ) == v{ 3 } ));
    assert(( run( "(block (var a 1) (var b 2) (block (var a 10) (set b (* a b)) (print b))"
                  " (print a) (print b))" ) == v{ 20, 1, 20 } ));
    assert(( run( "(block (var x 5) (block (var x (+ x 1)) (print x)) (print x))" ) ==
             v{ 6, 5 } ));

    auto error = []( std::string_view src ) -> std::string
    {
        try { run( src ); }
        catch ( const resolve_error &e ) { return e.what(); }
        return "";
    };

    assert( error( "(block (print 1) (print y))" ) == "undeclared variable y" );
    assert( error( "(block (var x 1) (var x 2))" ) == "redeclared variable x" );
    assert( error( "(block (block (var t 1)) (set t 2))" ) == "undeclared variable t" );

    return 0;
}
