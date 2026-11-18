#include <cassert>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

/* A compiler does not keep the nested structure of statements: the
 * machine only knows how to execute instructions one after another,
 * and how to «jump». In this exercise, you will «lower» the core
 * language (including ‹if› and ‹while›), extended with unlabelled
 * ‹(break)› and ‹(continue)›, into a flat list of instructions for a
 * simple «stack machine»:
 *
 *  • ‹push N› – push the integer ‹N› on the stack,
 *  • ‹load X› – push the value of variable ‹X›,
 *  • ‹store X› – pop a value and store it into variable ‹X›,
 *  • ‹op O› – pop ‹b›, pop ‹a›, push ‹a O b›, where ‹O› is one of the
 *    operators of the core language,
 *  • ‹print› – pop a value and append it to the output,
 *  • ‹label L› – does nothing; marks a jump target,
 *  • ‹jump L› – continue at ‹label L›,
 *  • ‹jz L› – pop a value; if it is zero, continue at ‹label L›.
 *
 * The machine has a single, flat set of variables, all of which start
 * as 0 – there are no scopes. Hence the compiler has to handle
 * scoping itself: every declaration must be compiled to its «own»
 * machine variable (e.g. by renaming the second ‹x› to ‹x.1›), so
 * that shadowing works as in the source language. Scopes are as in
 * ‹p2_resolve› (blocks and the bodies of ‹if› and ‹while›). Label and
 * variable names may be any words without spaces; each label must be
 * defined exactly once.
 *
 * Write a function ‹lower› which returns the instructions (one string
 * per instruction, words separated by single spaces). Return
 * ‹std::nullopt› if the program is malformed, uses an undeclared
 * variable, declares a variable twice in one scope, or has a ‹break›
 * or ‹continue› outside of a loop. Division by zero does not need to
 * be detected. The tests run your code on a machine which is given in
 * ‹main›. */

// std::optional< std::vector< std::string > > lower( std::string_view program );

/* The machine; returns the output, or ‹std::nullopt› if the code is
 * invalid or runs for too long. */

std::optional< std::vector< long > > execute( const std::vector< std::string > &code )
{
    std::map< std::string, std::size_t > labels;
    std::vector< std::vector< std::string > > ins;

    for ( const auto &line : code )
    {
        std::istringstream in( line );
        std::vector< std::string > w;
        for ( std::string x; in >> x; ) w.push_back( x );
        if ( w.empty() || w.size() > 2 ) return std::nullopt;
        if ( w[ 0 ] == "label" && ( w.size() != 2 || !labels.emplace( w[ 1 ], ins.size() ).second ) )
            return std::nullopt;
        ins.push_back( w );
    }

    std::map< std::string, long > vars;
    std::vector< long > stack, out;
    auto pop = [ & ] { if ( stack.empty() ) throw 0; long v = stack.back(); stack.pop_back(); return v; };

    try
    {
        for ( std::size_t pc = 0, steps = 0; pc < ins.size(); ++ steps )
        {
            if ( steps > 1000000 ) return std::nullopt;
            const auto &w = ins[ pc++ ];
            const std::string &op = w[ 0 ];
            auto arg = [ & ] { if ( w.size() != 2 ) throw 0; return w[ 1 ]; };
            auto target = [ & ] { auto it = labels.find( arg() ); if ( it == labels.end() ) throw 0; return it->second; };

            if ( op == "push" ) stack.push_back( std::stol( arg() ) );
            else if ( op == "load" ) stack.push_back( vars[ arg() ] );
            else if ( op == "store" ) vars[ arg() ] = pop();
            else if ( op == "print" ) out.push_back( pop() );
            else if ( op == "label" ) {}
            else if ( op == "jump" ) pc = target();
            else if ( op == "jz" ) { auto t = target(); if ( pop() == 0 ) pc = t; }
            else if ( op == "op" )
            {
                long b = pop(), a = pop();
                const std::string o = arg();
                long r = o == "+" ? a + b : o == "-" ? a - b : o == "*" ? a * b :
                         o == "/" ? a / b : o == "%" ? a % b : o == "<" ? a < b :
                         o == "<=" ? a <= b : o == ">" ? a > b : o == ">=" ? a >= b :
                         o == "=" ? a == b : o == "!=" ? a != b : throw 0;
                stack.push_back( r );
            }
            else return std::nullopt;
        }
    }
    catch ( int ) { return std::nullopt; }

    return out;
}

int main()
{
    using v = std::vector< long >;
    auto run = []( std::string_view p ) -> std::optional< v >
    {
        auto code = lower( p );
        assert( code );
        return execute( *code );
    };

    assert(( run( "(print (+ 1 (* 2 3)))" ) == v{ 7 } ));
    assert(( run( "(block (var x 2) (set x (* x x)) (print x))" ) == v{ 4 } ));
    assert(( run( "(block (var x 1) (block (var x 2) (print x) (set x 3)) (print x))" ) == v{ 2, 1 } ));
    assert(( run( "(block (var x 1) (if (> x 0) (print 10) (print 20)) (if 0 (print 30)))" ) == v{ 10 } ));
    assert(( run( "(block (var i 0) (while (< i 5) (block (set i (+ i 1))"
                  " (if (= i 2) (continue)) (if (= i 4) (break)) (print i))))" ) == v{ 1, 3 } ));
    assert(( run( "(block (var i 0) (while (< i 3) (block (var j 0) (set i (+ i 1))"
                  " (while 1 (block (set j (+ j 1)) (if (> j i) (break)) (print (+ (* 10 i) j)))))))" ) ==
             v{ 11, 21, 22, 31, 32, 33 } ));
    assert(( run( "(block (var n 10) (var a 0) (var b 1) (while n (block (print a)"
                  " (var t (+ a b)) (set a b) (set b t) (set n (- n 1)))))" ) ==
             v{ 0, 1, 1, 2, 3, 5, 8, 13, 21, 34 } ));
    assert(( run( "(block (var x 5) (while x (block (var x 0) (print x) (break))) (print x))" ) == v{ 0, 5 } ));

    assert( !lower( "(print y)" ) );
    assert( !lower( "(block (var x 1) (var x 2))" ) );
    assert( !lower( "(block (if 1 (var t 1)) (print t))" ) );
    assert( !lower( "(break)" ) );
    assert( !lower( "(if 1 (continue))" ) );
    assert( !lower( "(oops)" ) );

    return 0;
}
