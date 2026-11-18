#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
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

/* Java and C# reject programs which might read a local variable
 * before anything has been assigned to it. The check is called
 * «definite assignment analysis», and it is a nice example of a
 * conservative static analysis: a variable is «definitely assigned»
 * at some point if it has been assigned on «every» path through the
 * program which leads there.
 *
 * Extend the core language with ‹(var x)› – a declaration without an
 * initializer. Write a function ‹unassigned› which returns the names
 * of all «reads» of variables (atoms in expressions) at which the
 * variable is not definitely assigned, in program order, using these
 * rules:
 *
 *  • ‹(var x e)› assigns the new ‹x›, ‹(var x)› does not; ‹(set x e)›
 *    assigns ‹x› (after evaluating ‹e›),
 *  • after ‹(if c s₁ s₂)›, a variable is definitely assigned if it is
 *    definitely assigned after «both» ‹s₁› and ‹s₂›; an ‹if› without
 *    ‹else› behaves as if the missing branch did nothing,
 *  • the body of ‹(while c s)› may run zero times, so after the loop,
 *    exactly the variables which were definitely assigned before it
 *    are definitely assigned; the condition and the body are both
 *    analysed in the state from before the loop (assignments from
 *    previous iterations do not count),
 *  • the analysis does not look at the values of conditions: even
 *    ‹(if 1 …)› has two possible branches.
 *
 * Each declaration is a separate variable: an inner ‹x› which shadows
 * an outer one has its own assignment status. Scopes are as in
 * ‹p2_resolve› (blocks, and the bodies of ‹if› and ‹while›). You can
 * assume that the program is well formed and every variable is
 * declared before use. */

// std::vector< std::string > unassigned( std::string_view program );

/* Each declaration gets a unique number; the analysis state is the
 * set of numbers of the declarations which are definitely assigned. */

struct analysis
{
    std::vector< std::vector< std::pair< std::string, int > > > scopes{ {} };
    int next_id = 0;
    std::vector< std::string > out = {};

    int id( const std::string &n ) const
    {
        for ( auto s = scopes.rbegin(); s != scopes.rend(); ++ s )
            for ( auto &[ name, i ] : *s )
                if ( name == n )
                    return i;
        return -1;
    }

    void expr( const sexp &e, const std::set< int > &assigned )
    {
        if ( e.is_atom() )
        {
            if ( e.atom.find_first_not_of( "-0123456789" ) != e.atom.npos &&
                 !assigned.contains( id( e.atom ) ) )
                out.push_back( e.atom );
            return;
        }
        expr( e.items[ 1 ], assigned );
        expr( e.items[ 2 ], assigned );
    }

    /* Analyse ‹s› in a scope of its own. */
    std::set< int > scoped( const sexp &s, std::set< int > in )
    {
        scopes.emplace_back();
        auto r = stmt( s, std::move( in ) );
        scopes.pop_back();
        return r;
    }

    std::set< int > stmt( const sexp &s, std::set< int > in )
    {
        const std::string &head = s.items[ 0 ].atom;

        if ( head == "block" )
        {
            scopes.emplace_back();
            for ( std::size_t i = 1; i < s.items.size(); ++ i )
                in = stmt( s.items[ i ], std::move( in ) );
            scopes.pop_back();
        }
        else if ( head == "var" )
        {
            if ( s.items.size() == 3 )
                expr( s.items[ 2 ], in );
            int i = next_id++;
            scopes.back().emplace_back( s.items[ 1 ].atom, i );
            if ( s.items.size() == 3 )
                in.insert( i );
        }
        else if ( head == "set" )
        {
            expr( s.items[ 2 ], in );
            in.insert( id( s.items[ 1 ].atom ) );
        }
        else if ( head == "print" )
            expr( s.items[ 1 ], in );
        else if ( head == "if" )
        {
            expr( s.items[ 1 ], in );
            auto a = scoped( s.items[ 2 ], in );
            auto b = s.items.size() == 4 ? scoped( s.items[ 3 ], in ) : in;
            std::set< int > both;
            for ( int x : a )
                if ( b.contains( x ) )
                    both.insert( x );
            return both;
        }
        else /* while */
        {
            expr( s.items[ 1 ], in );
            scoped( s.items[ 2 ], in );
        }

        return in;
    }
};

std::vector< std::string > unassigned( std::string_view program )
{
    analysis a;
    a.stmt( read( program ), {} );
    return a.out;
}

int main()
{
    using v = std::vector< std::string >;

    assert(( unassigned( "(block (var x) (print x))" ) == v{ "x" } ));
    assert(( unassigned( "(block (var x) (set x 1) (print x))" ) == v{} ));
    assert(( unassigned( "(block (var x) (set x (+ x 1)))" ) == v{ "x" } ));
    assert(( unassigned( "(block (var c 1) (var x) (if c (set x 1) (set x 2)) (print x))" ) == v{} ));
    assert(( unassigned( "(block (var c 1) (var x) (if c (set x 1)) (print x))" ) == v{ "x" } ));
    assert(( unassigned( "(block (var c 1) (var x) (while c (block (set x 1) (set c 0))) (print x))" ) ==
             v{ "x" } ));
    assert(( unassigned( "(block (var c 1) (var x) (while c (block (print x) (set x 1))))" ) ==
             v{ "x" } ));
    assert(( unassigned( "(block (var x 1) (block (var x) (print x)) (print x))" ) == v{ "x" } ));
    assert(( unassigned( "(block (var x) (var y)"
                         " (if 1 (block (set x 1) (print x)) (print y)) (print x))" ) ==
             v{ "y", "x" } ));
    assert(( unassigned( "(block (var x) (var y) (set y x) (print (+ x y)))" ) == v{ "x", "x" } ));
    assert(( unassigned( "(block (var x) (if 1 (block (set x 1)) (if 0 (set x 2) (set x 3))) (print x))" ) ==
             v{} ));

    return 0;
}
