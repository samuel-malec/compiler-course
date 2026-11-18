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

/* Add «exceptions» to the core language (including ‹while›), together
 * with unlabelled ‹(break)› and ‹(continue)›:
 *
 *  • ‹(throw e)› – throw the value of ‹e› (an integer),
 *  • ‹(try s (catch x h))› – execute ‹s›; if it throws a value ‹v›,
 *    execute ‹h› in a new scope where ‹x› is declared with value ‹v›,
 *  • ‹(try s (finally f))› – execute ‹s› and then, «no matter how ‹s›
 *    completed» (normally, by throwing, or by ‹break› or ‹continue›),
 *    execute ‹f›; afterwards, continue the way ‹s› completed (e.g.
 *    keep propagating the exception) – «unless» ‹f› itself completes
 *    abnormally, in which case that completion replaces the original
 *    one,
 *  • ‹(try s (catch x h) (finally f))› – both: the ‹finally› part runs
 *    after ‹s› and (if it ran) ‹h›.
 *
 * An exception propagates out of blocks, ‹if›s and loops until it is
 * caught; one which reaches the top of the program stops it, and is
 * reported in the result as ‹uncaught›. Before running, check that
 * every ‹break› and ‹continue› is inside a loop (in the same sense as
 * in ‹p1_loops›), and that every ‹try› has the shape described above.
 * Return ‹std::nullopt› for these errors, for malformed programs and
 * for the run-time errors of ‹e1_print› (which are «not» exceptions
 * in this language, and cannot be caught). */

struct outcome
{
    std::vector< long > output;
    std::optional< long > uncaught;
};

// std::optional< outcome > run( std::string_view program );

struct bad {};

enum class kind { normal, brk, cont, thr };
struct completion { kind k = kind::normal; long value = 0; };

void check( const sexp &s, int loops )
{
    if ( s.is_atom() || s.items.empty() || !s.items[ 0 ].is_atom() )
        throw bad();
    const std::string &head = s.items[ 0 ].atom;
    auto n = s.items.size();

    if ( ( head == "break" || head == "continue" ) && ( n != 1 || !loops ) )
        throw bad();
    if ( head == "while" )
    {
        if ( n != 3 ) throw bad();
        check( s.items[ 2 ], loops + 1 );
    }
    else if ( head == "block" || head == "if" )
        for ( std::size_t i = head == "if" ? 2 : 1; i < n; ++ i )
            check( s.items[ i ], loops );
    else if ( head == "try" )
    {
        if ( n < 3 || n > 4 ) throw bad();
        check( s.items[ 1 ], loops );
        bool seen_catch = false, seen_finally = false;
        for ( std::size_t i = 2; i < n; ++ i )
        {
            const sexp &c = s.items[ i ];
            if ( !c.is_list() || c.items.empty() ) throw bad();
            if ( c.items[ 0 ].atom == "catch" && !seen_catch && !seen_finally &&
                 c.items.size() == 3 && c.items[ 1 ].is_atom() )
                seen_catch = true, check( c.items[ 2 ], loops );
            else if ( c.items[ 0 ].atom == "finally" && !seen_finally && c.items.size() == 2 )
                seen_finally = true, check( c.items[ 1 ], loops );
            else
                throw bad();
        }
    }
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

    /* Run ‹s› in a fresh scope, with an optional extra binding. */
    completion scoped( const sexp &s, const std::string &name = "", long v = 0 )
    {
        scopes.emplace_back();
        if ( !name.empty() )
            scopes.back()[ name ] = v;
        completion c = exec( s );
        scopes.pop_back();
        return c;
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
        if ( head == "break" ) return { kind::brk };
        if ( head == "continue" ) return { kind::cont };
        if ( head == "throw" && n == 2 ) return { kind::thr, eval( s.items[ 1 ] ) };
        if ( head == "while" )
        {
            while ( eval( s.items[ 1 ] ) )
            {
                completion c = exec( s.items[ 2 ] );
                if ( c.k == kind::brk ) break;
                if ( c.k == kind::thr ) return c;
            }
            return {};
        }
        if ( head == "if" )
        {
            if ( eval( s.items[ 1 ] ) )
                return exec( s.items[ 2 ] );
            return n == 4 ? exec( s.items[ 3 ] ) : completion{};
        }
        if ( head == "try" )
        {
            completion c = exec( s.items[ 1 ] );
            for ( std::size_t i = 2; i < n; ++ i )
            {
                const sexp &h = s.items[ i ];
                if ( h.items[ 0 ].atom == "catch" && c.k == kind::thr )
                    c = scoped( h.items[ 2 ], h.items[ 1 ].atom, c.value );
                else if ( h.items[ 0 ].atom == "finally" )
                    if ( completion f = exec( h.items[ 1 ] ); f.k != kind::normal )
                        c = f;
            }
            return c;
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

std::optional< outcome > run( std::string_view program )
{
    try
    {
        sexp p = read( program );
        check( p, 0 );
        machine m;
        completion c = m.exec( p );
        outcome o{ m.out, std::nullopt };
        if ( c.k == kind::thr )
            o.uncaught = c.value;
        return o;
    }
    catch ( const bad & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    using v = std::vector< long >;
    auto out = []( std::string_view p ) { return run( p )->output; };

    assert(( out( "(try (throw 5) (catch e (print e)))" ) == v{ 5 } ));
    assert(( out( "(block (try (block (print 1) (throw 2) (print 3))"
                  " (catch e (print (* e 10)))) (print 4))" ) == v{ 1, 20, 4 } ));
    assert(( out( "(try (try (throw 1) (catch e (throw (+ e 1)))) (catch e (print e)))" ) == v{ 2 } ));
    assert(( out( "(try (try (throw 1) (finally (print 9))) (catch e (print e)))" ) == v{ 9, 1 } ));
    assert(( out( "(try (print 1) (catch e (print e)) (finally (print 2)))" ) == v{ 1, 2 } ));
    assert(( out( "(try (throw 3) (catch e (print e)) (finally (print 2)))" ) == v{ 3, 2 } ));
    assert(( out( "(try (try (throw 1) (finally (throw 2))) (catch e (print e)))" ) == v{ 2 } ));

    assert(( out( "(block (var i 0) (while 1 (block (set i (+ i 1))"
                  " (try (if (= i 3) (break)) (finally (print i))))) (print 100))" ) ==
             v{ 1, 2, 3, 100 } ));
    assert(( out( "(block (var i 0) (while (< i 3) (block (set i (+ i 1))"
                  " (try (block (continue) (print 0)) (finally (print i))))))" ) ==
             v{ 1, 2, 3 } ));
    assert(( out( "(while 1 (try (throw 1) (finally (break))))" ) == v{} ));
    assert(( out( "(block (var i 0) (while (< i 9) (block (set i (+ i 1))"
                  " (try (if (= i 2) (throw i)) (catch x (break))))) (print i))" ) == v{ 2 } ));

    auto r = run( "(block (print 1) (throw 42) (print 2))" );
    assert( r && ( r->output == v{ 1 } ) && r->uncaught == 42 );
    assert( !run( "(print 1)" )->uncaught );

    assert( !run( "(block (try (throw 1) (catch e (print e))) (print e))" ) );
    assert( !run( "(try (throw (/ 1 0)) (catch e (print e)))" ) );
    assert( !run( "(break)" ) );
    assert( !run( "(try (print 1))" ) );
    assert( !run( "(try (print 1) (finally (print 2)) (catch e (print 3)))" ) );

    return 0;
}
