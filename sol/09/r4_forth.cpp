#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>

/* «Forth» is a stack language whose implementations are tiny compilers:
 * a definition is compiled, word by word, into a list of references to
 * other words (‘threaded code’), and executing it just runs through
 * the list. Write an interpreter for this subset of Forth. The input is
 * a sequence of words separated by whitespace (case-sensitive):
 *
 *  • an integer (with an optional ‹-›) pushes itself,
 *  • ‹+ - * /› and ‹mod› – arithmetic on the two top values (‹/› and
 *    ‹mod› truncate as in C++; division by zero is an error),
 *  • ‹= <› – comparison, pushes 1 (true) or 0 (false),
 *  • ‹dup drop swap over› – ‹( a -- a a )›, ‹( a -- )›, ‹( a b -- b a )›,
 *    ‹( a b -- a b a )›,
 *  • ‹.› – pop a value and append it to the output,
 *  • ‹: NAME … ;› – define a new word ‹NAME› (any whitespace-free text
 *    that is not an integer); the body is «compiled» at definition
 *    time: every word in it must already be defined (or be an integer,
 *    ‹NAME› itself for recursion, or one of the control words below),
 *    and it refers to the definition which is current «at that time» –
 *    redefining a word later does not change words compiled before,
 *  • inside a definition only: ‹if … then›, ‹if … else … then› (‹if›
 *    pops a value and runs the first part if it is non-zero), and
 *    ‹begin … until› (runs the body, then pops a value, and repeats if
 *    it is zero); these can be nested.
 *
 * Words outside definitions are executed immediately. User words may
 * redefine built-in words (except ‹:› and ‹;›). Return the output, or
 * ‹std::nullopt› on any error: an unknown word, a stack underflow, a
 * malformed definition (missing ‹;›, missing name, unbalanced control
 * words, control words outside a definition), division by zero, or
 * more than a million executed words. */

// std::optional< std::vector< long > > forth( std::string_view program );

/* A compiled definition is a list of «cells»: a literal, a call of a
 * built-in or of another definition (by pointer, so that redefining a
 * name later does not affect it), or a control cell with a jump target
 * (‹branch_if_zero› for ‹if› and ‹until›, ‹branch› for ‹else›). */

struct forth_error {};

struct definition;

struct cell
{
    enum kind { literal, builtin, call, branch0, branch } k;
    long value = 0;                       /* literal; jump target */
    std::string name = {};                /* built-in */
    const definition *target = nullptr;   /* call */
};

struct definition { std::vector< cell > body; };

struct machine
{
    std::vector< long > stack = {}, out = {};
    std::map< std::string, const definition * > words = {};
    std::vector< std::unique_ptr< definition > > defs = {};
    long steps = 0;

    static const std::set< std::string > &builtins()
    {
        static const std::set< std::string > b =
            { "+", "-", "*", "/", "mod", "=", "<", "dup", "drop", "swap", "over", "." };
        return b;
    }

    long pop()
    {
        if ( stack.empty() ) throw forth_error();
        long v = stack.back();
        stack.pop_back();
        return v;
    }

    void tick() { if ( ++ steps > 1000000 ) throw forth_error(); }

    void builtin( const std::string &w )
    {
        tick();
        if ( w == "." ) { out.push_back( pop() ); return; }
        if ( w == "dup" ) { long a = pop(); stack.push_back( a ), stack.push_back( a ); return; }
        if ( w == "drop" ) { pop(); return; }
        if ( w == "swap" ) { long b = pop(), a = pop(); stack.push_back( b ), stack.push_back( a ); return; }
        if ( w == "over" ) { long b = pop(), a = pop(); stack.push_back( a ), stack.push_back( b ), stack.push_back( a ); return; }
        long b = pop(), a = pop();
        if ( ( w == "/" || w == "mod" ) && b == 0 ) throw forth_error();
        stack.push_back( w == "+" ? a + b : w == "-" ? a - b : w == "*" ? a * b : w == "/" ? a / b :
                         w == "mod" ? a % b : w == "=" ? a == b : a < b );
    }

    void run( const definition &d )
    {
        for ( std::size_t pc = 0; pc < d.body.size(); )
        {
            const cell &c = d.body[ pc++ ];
            switch ( c.k )
            {
                case cell::literal: tick(); stack.push_back( c.value ); break;
                case cell::builtin: builtin( c.name ); break;
                case cell::call: tick(); run( *c.target ); break;
                case cell::branch0: tick(); if ( pop() == 0 ) pc = c.value; break;
                case cell::branch: pc = c.value; break;
            }
        }
    }

    static bool is_int( const std::string &w )
    {
        std::size_t s = w[ 0 ] == '-' ? 1 : 0;
        return w.size() > s && w.find_first_not_of( "0123456789", s ) == w.npos;
    }

    /* Compile one word into ‹d›, for the definition ‹self› of ‹name›. */
    cell compile_word( const std::string &w, const std::string &name, const definition *self )
    {
        if ( is_int( w ) ) return { cell::literal, std::stol( w ) };
        if ( w == name ) return { cell::call, 0, "", self };
        if ( auto it = words.find( w ); it != words.end() ) return { cell::call, 0, "", it->second };
        if ( builtins().contains( w ) ) return { cell::builtin, 0, w };
        throw forth_error();
    }

    void define( const std::vector< std::string > &toks, std::size_t &i )
    {
        if ( i >= toks.size() || toks[ i ] == ";" || is_int( toks[ i ] ) ) throw forth_error();
        std::string name = toks[ i++ ];
        defs.push_back( std::make_unique< definition >() );
        definition *d = defs.back().get();

        /* the control stack: positions of open ‹if› / ‹else› / ‹begin› */
        std::vector< std::pair< std::string, std::size_t > > control;

        while ( true )
        {
            if ( i >= toks.size() ) throw forth_error();
            const std::string &w = toks[ i++ ];
            auto &b = d->body;

            if ( w == ";" ) break;
            if ( w == ":" ) throw forth_error();
            if ( w == "if" )
            {
                control.emplace_back( "if", b.size() );
                b.push_back( { cell::branch0 } );
            }
            else if ( w == "else" )
            {
                if ( control.empty() || control.back().first != "if" ) throw forth_error();
                std::size_t at = control.back().second;
                control.back() = { "else", b.size() };
                b.push_back( { cell::branch } );
                b[ at ].value = b.size();
            }
            else if ( w == "then" )
            {
                if ( control.empty() || control.back().first == "begin" ) throw forth_error();
                b[ control.back().second ].value = b.size();
                control.pop_back();
            }
            else if ( w == "begin" )
                control.emplace_back( "begin", b.size() );
            else if ( w == "until" )
            {
                if ( control.empty() || control.back().first != "begin" ) throw forth_error();
                b.push_back( { cell::branch0, long( control.back().second ) } );
                control.pop_back();
            }
            else
                b.push_back( compile_word( w, name, d ) );
        }

        if ( !control.empty() ) throw forth_error();
        words[ name ] = d;
    }

    void interpret( const std::vector< std::string > &toks )
    {
        for ( std::size_t i = 0; i < toks.size(); )
        {
            const std::string &w = toks[ i++ ];
            if ( w == ":" ) { define( toks, i ); continue; }
            if ( w == ";" || w == "if" || w == "else" || w == "then" || w == "begin" || w == "until" )
                throw forth_error();
            if ( is_int( w ) ) { stack.push_back( std::stol( w ) ); continue; }
            if ( auto it = words.find( w ); it != words.end() ) { run( *it->second ); continue; }
            if ( builtins().contains( w ) ) { builtin( w ); continue; }
            throw forth_error();
        }
    }
};

std::optional< std::vector< long > > forth( std::string_view program )
{
    std::istringstream in{ std::string( program ) };
    std::vector< std::string > toks;
    for ( std::string w; in >> w; ) toks.push_back( w );

    machine m;
    try { m.interpret( toks ); }
    catch ( const forth_error & ) { return std::nullopt; }
    return m.out;
}

int main()
{
    using v = std::vector< long >;

    assert(( forth( "1 2 + ." ) == v{ 3 } ));
    assert(( forth( "10 3 - . 7 2 / . -7 2 mod ." ) == v{ 7, 3, -1 } ));
    assert(( forth( "1 2 swap . . 5 dup * . 1 2 over . . . 4 drop" ) == v{ 1, 2, 25, 1, 2, 1 } ));
    assert(( forth( ": square dup * ; 7 square ." ) == v{ 49 } ));
    assert(( forth( ": abs dup 0 < if 0 swap - then ; -5 abs . 5 abs ." ) == v{ 5, 5 } ));
    assert(( forth( ": sign dup 0 < if drop -1 else 0 = if 0 else 1 then then ; -3 sign . 0 sign . 9 sign ." ) ==
             v{ -1, 0, 1 } ));
    assert(( forth( ": countdown begin dup . 1 - dup 0 = until drop ; 3 countdown" ) == v{ 3, 2, 1 } ));
    assert(( forth( ": fact dup 1 < if drop 1 else dup 1 - fact * then ; 10 fact ." ) == v{ 3628800 } ));

    /* early binding: ‹bar› keeps using the old ‹foo› */
    assert(( forth( ": foo 1 ; : bar foo ; : foo 2 ; bar . foo ." ) == v{ 1, 2 } ));
    assert(( forth( ": + * ; 3 4 + ." ) == v{ 12 } ));

    assert( !forth( "1 +" ) );
    assert( !forth( "frob" ) );
    assert( !forth( ": broken frob ;" ) );
    assert( !forth( ": nosemi 1 2" ) );
    assert( !forth( ": ;" ) );
    assert( !forth( ": bad if 1 ;" ) );
    assert( !forth( "1 if 2 then" ) );
    assert( !forth( "1 0 /" ) );
    assert( !forth( ": loop begin 0 until ; loop" ) );

    return 0;
}
