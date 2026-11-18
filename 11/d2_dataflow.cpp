#include <algorithm>
#include <cassert>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* The three-address code (TAC) of this chapter (see ‹d1_cfg›). A
 * program is a list of lines, each one instruction:
 *
 *     x = a           ; a copy (‹a› is a variable or an integer)
 *     x = a op b      ; op is one of + - * / % < <= == !=
 *     L:              ; a label
 *     goto L
 *     if a goto L     ; jump if ‹a› is not zero
 *     print a
 *     x = phi L1:a L2:b …   ; (SSA only) the value of ‹a› if control
 *                           ; came from the block labelled L1, …
 *
 * Variables are names starting with a lowercase letter (letters,
 * digits, ‹_› and ‹.›); labels start with an uppercase letter. ‹parse›
 * turns lines into ‹instr›s (throwing ‹std::invalid_argument› on a
 * malformed line), ‹show› does the opposite. ‹run› interprets a
 * program (given as text; ‹run_ir› takes parsed instructions): variables start as 0, and a ‹phi› picks the operand for the
 * label of the block which was executed «before» the current one (the
 * last label passed before the jump or fall-through which entered the
 * block containing the ‹phi›; the entry block, if it has no label, is
 * called ‹Entry›). It returns the printed values, or ‹std::nullopt› on
 * division by zero, a jump to an unknown label, a ‹phi› without an
 * operand for the predecessor, or after ‹fuel› instructions. */

struct instr
{
    std::string kind;              /* "set", "label", "goto", "if", "print", "phi" */
    std::string dst = {};          /* set, phi */
    std::string a = {}, op = {}, b = {};   /* set: a [op b]; if, print: a */
    std::string target = {};       /* goto, if: the label; label: its name */
    std::vector< std::pair< std::string, std::string > > phi = {};

    bool operator==( const instr & ) const = default;
};

bool is_var( const std::string &s ) { return !s.empty() && s[ 0 ] >= 'a' && s[ 0 ] <= 'z'; }
bool is_label( const std::string &s ) { return !s.empty() && s[ 0 ] >= 'A' && s[ 0 ] <= 'Z'; }
bool is_int( const std::string &s )
{
    std::size_t k = !s.empty() && s[ 0 ] == '-' ? 1 : 0;
    return s.size() > k && s.find_first_not_of( "0123456789", k ) == s.npos;
}
bool is_operand( const std::string &s ) { return is_var( s ) || is_int( s ); }

instr parse_line( std::string_view line )
{
    std::vector< std::string > w;
    std::string cur;
    for ( char c : line )
        if ( c == ' ' || c == '\t' ) { if ( !cur.empty() ) w.push_back( cur ), cur.clear(); }
        else cur += c;
    if ( !cur.empty() ) w.push_back( cur );

    auto bad = [] { return std::invalid_argument( "malformed TAC" ); };
    static const std::set< std::string > ops = { "+", "-", "*", "/", "%", "<", "<=", "==", "!=" };

    if ( w.size() == 1 && w[ 0 ].back() == ':' && is_label( w[ 0 ].substr( 0, w[ 0 ].size() - 1 ) ) )
        return { "label", "", "", "", "", w[ 0 ].substr( 0, w[ 0 ].size() - 1 ) };
    if ( w.size() == 2 && w[ 0 ] == "goto" && is_label( w[ 1 ] ) ) return { "goto", "", "", "", "", w[ 1 ] };
    if ( w.size() == 4 && w[ 0 ] == "if" && is_operand( w[ 1 ] ) && w[ 2 ] == "goto" && is_label( w[ 3 ] ) )
        return { "if", "", w[ 1 ], "", "", w[ 3 ] };
    if ( w.size() == 2 && w[ 0 ] == "print" && is_operand( w[ 1 ] ) ) return { "print", "", w[ 1 ] };
    if ( w.size() >= 3 && is_var( w[ 0 ] ) && w[ 1 ] == "=" )
    {
        if ( w[ 2 ] == "phi" )
        {
            instr i{ "phi", w[ 0 ] };
            for ( std::size_t k = 3; k < w.size(); ++ k )
            {
                auto colon = w[ k ].find( ':' );
                if ( colon == std::string::npos ) throw bad();
                std::string l = w[ k ].substr( 0, colon ), v = w[ k ].substr( colon + 1 );
                if ( !is_label( l ) || !is_operand( v ) ) throw bad();
                i.phi.emplace_back( l, v );
            }
            return i;
        }
        if ( w.size() == 3 && is_operand( w[ 2 ] ) ) return { "set", w[ 0 ], w[ 2 ] };
        if ( w.size() == 5 && is_operand( w[ 2 ] ) && ops.contains( w[ 3 ] ) && is_operand( w[ 4 ] ) )
            return { "set", w[ 0 ], w[ 2 ], w[ 3 ], w[ 4 ] };
    }
    throw bad();
}

std::vector< instr > parse( const std::vector< std::string > &lines )
{
    std::vector< instr > out;
    for ( const auto &l : lines ) out.push_back( parse_line( l ) );
    return out;
}

std::string show( const instr &i )
{
    if ( i.kind == "label" ) return i.target + ":";
    if ( i.kind == "goto" ) return "goto " + i.target;
    if ( i.kind == "if" ) return "if " + i.a + " goto " + i.target;
    if ( i.kind == "print" ) return "print " + i.a;
    if ( i.kind == "phi" )
    {
        std::string s = i.dst + " = phi";
        for ( const auto &[ l, v ] : i.phi ) s += " " + l + ":" + v;
        return s;
    }
    return i.dst + " = " + i.a + ( i.op.empty() ? "" : " " + i.op + " " + i.b );
}

std::vector< std::string > show( const std::vector< instr > &p )
{
    std::vector< std::string > out;
    for ( const auto &i : p ) out.push_back( show( i ) );
    return out;
}

/* the variables read and written by an instruction */
std::vector< std::string > uses( const instr &i )
{
    std::vector< std::string > out;
    for ( const auto &x : { i.a, i.b } ) if ( is_var( x ) ) out.push_back( x );
    for ( const auto &[ _, v ] : i.phi ) if ( is_var( v ) ) out.push_back( v );
    return out;
}

std::optional< std::string > def( const instr &i )
{
    if ( i.kind == "set" || i.kind == "phi" ) return i.dst;
    return std::nullopt;
}

long apply_op( const std::string &op, long x, long y )
{
    if ( op == "+" ) return x + y;
    if ( op == "-" ) return x - y;
    if ( op == "*" ) return x * y;
    if ( op == "/" ) return x / y;
    if ( op == "%" ) return x % y;
    if ( op == "<" ) return x < y;
    if ( op == "<=" ) return x <= y;
    if ( op == "==" ) return x == y;
    return x != y;
}

std::optional< std::vector< long > > run_ir( const std::vector< instr > &p, long fuel = 100000 )
{
    std::map< std::string, std::size_t > labels;
    for ( std::size_t k = 0; k < p.size(); ++ k )
        if ( p[ k ].kind == "label" ) labels[ p[ k ].target ] = k;

    std::map< std::string, long > vars;
    std::vector< long > out;
    std::string current = "Entry", previous;
    auto val = [ & ]( const std::string &s ) { return is_int( s ) ? std::stol( s ) : vars[ s ]; };

    for ( std::size_t pc = 0; pc < p.size(); )
    {
        if ( -- fuel < 0 ) return std::nullopt;
        const instr &i = p[ pc++ ];

        if ( i.kind == "label" ) { previous = current; current = i.target; }
        else if ( i.kind == "print" ) out.push_back( val( i.a ) );
        else if ( i.kind == "goto" || ( i.kind == "if" && val( i.a ) != 0 ) )
        {
            auto it = labels.find( i.target );
            if ( it == labels.end() ) return std::nullopt;
            pc = it->second;
        }
        else if ( i.kind == "phi" )
        {
            /* all the phis at the start of a block read their operands
             * «before» any of them writes (they happen in parallel) */
            std::vector< std::pair< std::string, long > > writes;
            for ( pc = pc - 1; pc < p.size() && p[ pc ].kind == "phi"; ++ pc )
            {
                bool found = false;
                for ( const auto &[ l, v ] : p[ pc ].phi )
                    if ( l == previous ) { writes.emplace_back( p[ pc ].dst, val( v ) ); found = true; break; }
                if ( !found ) return std::nullopt;
            }
            for ( auto &[ x, v ] : writes ) vars[ x ] = v;
        }
        else if ( i.kind == "set" )
        {
            if ( i.op.empty() ) vars[ i.dst ] = val( i.a );
            else
            {
                long x = val( i.a ), y = val( i.b );
                if ( ( i.op == "/" || i.op == "%" ) && y == 0 ) return std::nullopt;
                vars[ i.dst ] = apply_op( i.op, x, y );
            }
        }
    }

    return out;
}

std::optional< std::vector< long > > run( const std::vector< std::string > &lines, long fuel = 100000 )
{
    return run_ir( parse( lines ), fuel );
}

/* The control flow graph (see ‹d1_cfg›). ‹build_cfg› splits a program
 * into «basic blocks»: maximal sequences of instructions which are
 * always executed from the first to the last. A new block starts at the
 * first instruction, at every label, and after every ‹goto› and ‹if›.
 * A block keeps its label (the label line itself is not a part of
 * ‹code›); a block without one gets the name ‹@N› (‹N› its index), and
 * the first block, if unlabelled, is ‹Entry› (as in ‹run›). The
 * successors of a block are the target of its final ‹goto› or ‹if›, and
 * – unless it ends with ‹goto› – the next block. Block 0 is the entry.
 * ‹flatten› turns the blocks back into a program, in their order,
 * emitting a label line for every block with ‹labelled› set (a new
 * block which is the target of a jump must have it). A jump to a label
 * which does not exist throws ‹std::invalid_argument›. */

struct block
{
    std::string label;
    bool labelled;                       /* did the source have a label line? */
    std::vector< instr > code;
    std::vector< std::size_t > succs = {}, preds = {};
};

std::vector< block > build_cfg( const std::vector< instr > &p )
{
    std::vector< block > bs;
    bool start = true;

    for ( const auto &i : p )
    {
        if ( i.kind == "label" )
        {
            bs.push_back( { i.target, true, {} } );
            start = false;
            continue;
        }
        if ( start )
        {
            bs.push_back( { bs.empty() ? "Entry" : "@" + std::to_string( bs.size() ), false, {} } );
            start = false;
        }
        bs.back().code.push_back( i );
        if ( i.kind == "goto" || i.kind == "if" ) start = true;
    }
    if ( bs.empty() ) bs.push_back( { "Entry", false, {} } );

    std::map< std::string, std::size_t > index;
    for ( std::size_t k = 0; k < bs.size(); ++ k ) index[ bs[ k ].label ] = k;

    for ( std::size_t k = 0; k < bs.size(); ++ k )
    {
        const auto &c = bs[ k ].code;
        bool ends_goto = !c.empty() && c.back().kind == "goto";
        if ( !c.empty() && ( c.back().kind == "goto" || c.back().kind == "if" ) )
        {
            auto it = index.find( c.back().target );
            if ( it == index.end() ) throw std::invalid_argument( "unknown label" );
            bs[ k ].succs.push_back( it->second );
        }
        if ( !ends_goto && k + 1 < bs.size() &&
             std::find( bs[ k ].succs.begin(), bs[ k ].succs.end(), k + 1 ) == bs[ k ].succs.end() )
            bs[ k ].succs.push_back( k + 1 );
    }

    for ( std::size_t k = 0; k < bs.size(); ++ k )
        for ( auto s : bs[ k ].succs ) bs[ s ].preds.push_back( k );

    return bs;
}

std::vector< instr > flatten( const std::vector< block > &bs )
{
    std::vector< instr > out;
    for ( const auto &b : bs )
    {
        if ( b.labelled ) out.push_back( { "label", "", "", "", "", b.label } );
        out.insert( out.end(), b.code.begin(), b.code.end() );
    }
    return out;
}

/* This demo implements a generic «data-flow solver» and uses it for
 * two classic analyses. The values are sets of strings, which covers
 * a lot of analyses (sets of variables, of definitions, of
 * expressions); the solver is parametrized by:
 *
 *  • the «direction»: forward analyses propagate information along the
 *    edges (from the entry), backward ones against them (from the
 *    exits),
 *  • the «meet» of the values coming from several neighbours: «union»
 *    for ‘may’ analyses (true on «some» path), «intersection» for ‘must’
 *    analyses (true on «all» paths),
 *  • the «transfer function» of a block: given the value at its start
 *    (forward) or end (backward), the value at the other end,
 *  • the value at the boundary (the entry, or the exits) and the
 *    initial value of all the other points.
 *
 * The solver keeps a «worklist» of blocks whose input may have
 * changed; it takes a block, recomputes its input from its neighbours
 * and its output with the transfer function, and if the output
 * changed, it adds the blocks which depend on it to the worklist. With
 * monotone transfer functions over a finite lattice, this reaches the
 * (least or greatest) fixpoint. */

using value = std::set< std::string >;

struct problem
{
    bool forward;
    bool may;                                   /* union (true) or intersection */
    value boundary, initial;
    std::function< value( std::size_t, const value & ) > transfer;
};

struct solution { std::vector< value > in, out; };   /* in = at the start of a block */

solution solve( const std::vector< block > &bs, const problem &pr )
{
    std::size_t n = bs.size();
    solution s{ std::vector< value >( n, pr.initial ), std::vector< value >( n, pr.initial ) };

    /* ‘before’ is the side the information comes from */
    auto &before = pr.forward ? s.in : s.out;
    auto &after = pr.forward ? s.out : s.in;
    auto sources = [ & ]( std::size_t b ) -> const std::vector< std::size_t > & { return pr.forward ? bs[ b ].preds : bs[ b ].succs; };
    auto targets = [ & ]( std::size_t b ) -> const std::vector< std::size_t > & { return pr.forward ? bs[ b ].succs : bs[ b ].preds; };

    std::vector< std::size_t > work;
    for ( std::size_t b = 0; b < n; ++ b ) work.push_back( n - 1 - b );

    while ( !work.empty() )
    {
        std::size_t b = work.back();
        work.pop_back();

        value v;
        bool boundary = pr.forward ? b == 0 : bs[ b ].succs.empty();
        if ( boundary ) v = pr.boundary;
        bool first = !boundary;
        for ( auto p : sources( b ) )
        {
            if ( first ) { v = after[ p ]; first = false; continue; }
            value m;
            if ( pr.may ) std::set_union( v.begin(), v.end(), after[ p ].begin(), after[ p ].end(), std::inserter( m, m.end() ) );
            else std::set_intersection( v.begin(), v.end(), after[ p ].begin(), after[ p ].end(), std::inserter( m, m.end() ) );
            v = m;
        }
        before[ b ] = v;

        value out = pr.transfer( b, v );
        if ( out != after[ b ] )
        {
            after[ b ] = out;
            for ( auto t : targets( b ) )
                if ( std::find( work.begin(), work.end(), t ) == work.end() ) work.push_back( t );
        }
    }

    return s;
}

/* «Reaching definitions» (forward, may): a definition is an
 * instruction which assigns a variable; we name it ‹x@k›, where ‹k› is
 * the index of the block and the position in it, ‹B.i›. A block «kills»
 * all definitions of the variables it assigns, and «generates» its own
 * last definition of each. */

solution reaching( const std::vector< block > &bs )
{
    std::map< std::string, value > defs_of;   /* variable → all its definitions */
    for ( std::size_t b = 0; b < bs.size(); ++ b )
        for ( std::size_t i = 0; i < bs[ b ].code.size(); ++ i )
            if ( auto d = def( bs[ b ].code[ i ] ) )
                defs_of[ *d ].insert( *d + "@" + bs[ b ].label + "." + std::to_string( i ) );

    return solve( bs, { true, true, {}, {}, [ & ]( std::size_t b, const value &in )
    {
        value v = in;
        for ( std::size_t i = 0; i < bs[ b ].code.size(); ++ i )
            if ( auto d = def( bs[ b ].code[ i ] ) )
            {
                for ( const auto &old : defs_of[ *d ] ) v.erase( old );
                v.insert( *d + "@" + bs[ b ].label + "." + std::to_string( i ) );
            }
        return v;
    } } );
}

/* «Liveness» (backward, may): a variable is live if it may be read
 * later, before being assigned. Going backwards through a block, an
 * assignment makes the variable dead, a use makes it live. */

solution liveness( const std::vector< block > &bs )
{
    return solve( bs, { false, true, {}, {}, [ & ]( std::size_t b, const value &out )
    {
        value v = out;
        for ( auto it = bs[ b ].code.rbegin(); it != bs[ b ].code.rend(); ++ it )
        {
            if ( auto d = def( *it ) ) v.erase( *d );
            for ( const auto &u : uses( *it ) ) v.insert( u );
        }
        return v;
    } } );
}

int main() /* demo */
{
    auto bs = build_cfg( parse( {
        "s = 0",              /* Entry.0 */
        "i = 1",              /* Entry.1 */
        "Loop:",
        "c = i <= 10",        /* Loop.0 */
        "if c goto Body",
        "goto Done",
        "Body:",
        "s = s + i",          /* Body.0 */
        "i = i + 1",          /* Body.1 */
        "goto Loop",
        "Done:",
        "print s",
    } ) );

    using v = value;
    auto r = reaching( bs );
    /* at the loop header, both the initial and the in-loop definitions
     * reach – including ‹c› from the previous iteration */
    assert(( r.in[ 1 ] == v{ "c@Loop.0", "i@Body.1", "i@Entry.1", "s@Body.0", "s@Entry.0" } ));
    assert(( r.in[ 0 ] == v{} ));
    assert(( r.in[ 4 ] == v{ "c@Loop.0", "i@Body.1", "i@Entry.1", "s@Body.0", "s@Entry.0" } ));

    auto l = liveness( bs );
    assert(( l.in[ 0 ] == v{} ));              /* nothing is read before it is written */
    assert(( l.in[ 1 ] == v{ "i", "s" } ));
    assert(( l.out[ 3 ] == v{ "i", "s" } ));    /* Body: both are needed in the next iteration */
    assert(( l.in[ 4 ] == v{ "s" } ));          /* Done */

    return 0;
}
