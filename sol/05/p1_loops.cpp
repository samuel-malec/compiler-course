#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>

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

/* Write an interpreter for the full core language from the
 * introduction (including ‹if› and ‹while›), extended with these
 * statements:
 *
 *  • ‹(break)› – leave the innermost enclosing loop,
 *  • ‹(continue)› – skip the rest of the body of the innermost loop and
 *    go on with the next iteration (evaluating the condition again),
 *  • ‹(label L (while c s))› – a loop with a name, ‹L›,
 *  • ‹(break L)› and ‹(continue L)› – the same as above, but for the
 *    enclosing loop named ‹L›, which may be several loops further out.
 *
 * Before running the program, check it: it is an error if ‹break› or
 * ‹continue› appears outside of any loop, if a labelled ‹break› or
 * ‹continue› refers to a label which is not on an enclosing loop, if
 * ‹label› is applied to anything other than ‹while›, or if a labelled
 * loop is nested in another loop with the same label. Return
 * ‹std::nullopt› for these errors, for malformed programs and for the
 * run-time errors of ‹e1_print›; otherwise return the printed values.
 *
 * Hint: let ‹exec› return a «completion» (as in ‹d1_stmt›), which, for
 * ‹break› and ‹continue›, also carries the label (if any). */

// std::optional< std::vector< long > > run( std::string_view program );

struct bad {};

enum class kind { normal, brk, cont };
struct completion { kind k = kind::normal; std::string label = {}; };

/* The static check: walk the program with the stack of enclosing
 * loops (their labels, "" for unlabelled ones). */

void check( const sexp &s, std::vector< std::string > &loops )
{
    if ( s.is_atom() || s.items.empty() || !s.items[ 0 ].is_atom() )
        throw bad();
    const std::string &head = s.items[ 0 ].atom;
    auto n = s.items.size();

    auto has = [ & ]( const std::string &l )
    {
        for ( const auto &x : loops ) if ( x == l ) return true;
        return false;
    };

    if ( head == "break" || head == "continue" )
    {
        if ( loops.empty() || n > 2 || ( n == 2 && !has( s.items[ 1 ].atom ) ) )
            throw bad();
    }
    else if ( head == "label" )
    {
        if ( n != 3 || !s.items[ 1 ].is_atom() || !s.items[ 2 ].is_list() ||
             s.items[ 2 ].items.empty() || s.items[ 2 ].items[ 0 ].atom != "while" ||
             has( s.items[ 1 ].atom ) )
            throw bad();
        loops.push_back( s.items[ 1 ].atom );
        check( s.items[ 2 ].items.at( 2 ), loops );
        loops.pop_back();
    }
    else if ( head == "while" )
    {
        if ( n != 3 ) throw bad();
        loops.push_back( "" );
        check( s.items[ 2 ], loops );
        loops.pop_back();
    }
    else if ( head == "if" )
    {
        if ( n != 3 && n != 4 ) throw bad();
        for ( std::size_t i = 2; i < n; ++ i )
            check( s.items[ i ], loops );
    }
    else if ( head == "block" )
        for ( std::size_t i = 1; i < n; ++ i )
            check( s.items[ i ], loops );
}

struct machine
{
    std::vector< std::map< std::string, long > > scopes{ {} };
    std::vector< long > out = {};

    long *find( const std::string &n )
    {
        for ( auto it = scopes.rbegin(); it != scopes.rend(); ++ it )
            if ( auto v = it->find( n ); v != it->end() )
                return &v->second;
        throw bad();
    }

    long eval( const sexp &e )
    {
        if ( e.is_atom() )
        {
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos && e.atom != "-" )
                return std::stol( e.atom );
            return *find( e.atom );
        }
        if ( e.items.size() != 3 || !e.items[ 0 ].is_atom() )
            throw bad();
        const std::string &op = e.items[ 0 ].atom;
        long a = eval( e.items[ 1 ] ), b = eval( e.items[ 2 ] );
        if ( op == "+" ) return a + b;
        if ( op == "-" ) return a - b;
        if ( op == "*" ) return a * b;
        if ( op == "<" ) return a < b;
        if ( op == "<=" ) return a <= b;
        if ( op == ">" ) return a > b;
        if ( op == ">=" ) return a >= b;
        if ( op == "=" ) return a == b;
        if ( op == "!=" ) return a != b;
        if ( ( op == "/" || op == "%" ) && b != 0 ) return op == "/" ? a / b : a % b;
        throw bad();
    }

    /* Run a loop with the given label ("" if none). A completion whose
     * label is empty or equal to ours is consumed here; any other one
     * is passed outwards. */
    completion loop( const sexp &w, const std::string &label )
    {
        while ( eval( w.items[ 1 ] ) )
        {
            completion c = exec( w.items[ 2 ] );
            if ( c.k == kind::normal )
                continue;
            if ( !c.label.empty() && c.label != label )
                return c;
            if ( c.k == kind::brk )
                break;
        }
        return {};
    }

    completion exec( const sexp &s )
    {
        const std::string &head = s.items[ 0 ].atom;
        auto n = s.items.size();

        if ( head == "block" )
        {
            scopes.emplace_back();
            completion c;
            for ( std::size_t i = 1; i < n && c.k == kind::normal; ++ i )
                c = exec( s.items[ i ] );
            scopes.pop_back();
            return c;
        }
        if ( head == "break" || head == "continue" )
            return { head == "break" ? kind::brk : kind::cont,
                     n == 2 ? s.items[ 1 ].atom : "" };
        if ( head == "label" )
            return loop( s.items[ 2 ], s.items[ 1 ].atom );
        if ( head == "while" )
            return loop( s, "" );
        if ( head == "if" )
        {
            if ( eval( s.items[ 1 ] ) )
                return exec( s.items[ 2 ] );
            return n == 4 ? exec( s.items[ 3 ] ) : completion{};
        }

        if ( head == "var" && n == 3 && s.items[ 1 ].is_atom() )
        {
            long v = eval( s.items[ 2 ] );
            if ( !scopes.back().emplace( s.items[ 1 ].atom, v ).second )
                throw bad();
        }
        else if ( head == "set" && n == 3 && s.items[ 1 ].is_atom() )
        {
            long v = eval( s.items[ 2 ] );
            *find( s.items[ 1 ].atom ) = v;
        }
        else if ( head == "print" && n == 2 )
            out.push_back( eval( s.items[ 1 ] ) );
        else
            throw bad();

        return {};
    }
};

std::optional< std::vector< long > > run( std::string_view program )
{
    try
    {
        sexp p = read( program );
        std::vector< std::string > loops;
        check( p, loops );
        machine m;
        m.exec( p );
        return m.out;
    }
    catch ( const bad & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
    catch ( const std::out_of_range & ) { return std::nullopt; }
}

int main()
{
    using v = std::vector< long >;

    assert(( run( "(block (var i 0) (while (< i 10) (block (set i (+ i 1))"
                  "  (if (= (% i 2) 0) (continue)) (if (> i 7) (break)) (print i))))" ) ==
             v{ 1, 3, 5, 7 } ));

    auto pairs = "(block (var i 0)"
                 " (label outer (while (< i 3) (block (var j 0) (set i (+ i 1))"
                 "   (while 1 (block (set j (+ j 1))"
                 "     (if (= j 2) (continue outer))"
                 "     (if (= i 3) (break outer))"
                 "     (print (+ (* 10 i) j))))))))";
    assert(( run( pairs ) == v{ 11, 21 } ));

    assert(( run( "(block (var n 0) (label a (while (< n 3) (block (set n (+ n 1))"
                  " (label b (while 1 (break b))) (print n)))))" ) == v{ 1, 2, 3 } ));
    assert(( run( "(if 0 (print 1) (print 2))" ) == v{ 2 } ));

    assert( !run( "(break)" ) );
    assert( !run( "(block (if 1 (continue)))" ) );
    assert( !run( "(while 0 (break nope))" ) );
    assert( !run( "(label x (print 1))" ) );
    assert( !run( "(label x (while 0 (label x (while 0 (break)))))" ) );
    assert( !run( "(block (label x (while 0 (break))) (while 0 (break x)))" ) );
    assert( !run( "(while 1 (print (/ 1 0)))" ) );

    return 0;
}
