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

/* This demo converts a program into «static single assignment» form,
 * in three steps (the algorithm of Cytron et al., 1991):
 *
 *  1. compute the «dominators» of each block, and from them the
 *     «dominator tree» and the «dominance frontiers»,
 *  2. insert φ-functions: a variable assigned in block ‹B› needs a φ
 *     in every block of the dominance frontier of ‹B› – the first blocks
 *     where the definition in ‹B› «meets» other definitions – and,
 *     since a φ is itself a definition, iterate,
 *  3. «rename» the variables: walk the dominator tree, keeping for each
 *     variable a stack of its current SSA names; every definition
 *     creates a new name ‹x.N›, every use refers to the top of the stack,
 *     and the φ operands in successor blocks get the name current at
 *     the end of the predecessor.
 *
 * A variable which is read before being assigned gets the name ‹x.0›,
 * which the interpreter treats as 0, like any unassigned variable.
 *
 * ── Dominators ───────────────────────────────────────────────────── */

/* ‹idom[ b ]› is the «immediate dominator» of ‹b› (the closest strict
 * dominator), ‹-1› for the entry and for unreachable blocks. We use the
 * simple iterative algorithm of Cooper, Harvey and Kennedy: process the
 * blocks in reverse postorder, and set the idom of each to the
 * «intersection» (the nearest common ancestor in the current tree) of
 * its already processed predecessors, until nothing changes. */

std::vector< int > dominators( const std::vector< block > &bs )
{
    std::size_t n = bs.size();
    std::vector< int > order, number( n, -1 );
    std::vector< bool > seen( n, false );
    std::function< void( std::size_t ) > dfs = [ & ]( std::size_t b )
    {
        seen[ b ] = true;
        for ( auto s : bs[ b ].succs ) if ( !seen[ s ] ) dfs( s );
        order.push_back( b );                     /* postorder */
    };
    dfs( 0 );
    std::reverse( order.begin(), order.end() );   /* reverse postorder */
    for ( std::size_t k = 0; k < order.size(); ++ k ) number[ order[ k ] ] = k;

    std::vector< int > idom( n, -1 );
    idom[ 0 ] = 0;

    auto intersect = [ & ]( int a, int b )
    {
        while ( a != b )
        {
            while ( number[ a ] > number[ b ] ) a = idom[ a ];
            while ( number[ b ] > number[ a ] ) b = idom[ b ];
        }
        return a;
    };

    for ( bool changed = true; changed; )
    {
        changed = false;
        for ( std::size_t k = 1; k < order.size(); ++ k )
        {
            int b = order[ k ], next = -1;
            for ( auto p : bs[ b ].preds )
                if ( idom[ p ] >= 0 ) next = next < 0 ? int( p ) : intersect( p, next );
            if ( next != idom[ b ] ) idom[ b ] = next, changed = true;
        }
    }

    idom[ 0 ] = -1;
    return idom;
}

/* The dominance frontier of ‹b›: the blocks ‹y› such that ‹b› dominates a
 * predecessor of ‹y› but does not strictly dominate ‹y›. For each join
 * point ‹y›, walk up the tree from each predecessor until ‹idom( y )›. */

std::vector< std::set< std::size_t > > frontiers( const std::vector< block > &bs, const std::vector< int > &idom )
{
    std::vector< std::set< std::size_t > > df( bs.size() );
    for ( std::size_t y = 0; y < bs.size(); ++ y )
        if ( bs[ y ].preds.size() >= 2 )
            for ( auto p : bs[ y ].preds )
                for ( int r = p; r >= 0 && r != idom[ y ]; r = idom[ r ] )
                {
                    df[ r ].insert( y );
                    if ( r == 0 ) break;
                }
    return df;
}

/* ── SSA construction ─────────────────────────────────────────────── */

std::vector< instr > to_ssa( const std::vector< instr > &program )
{
    auto bs = build_cfg( program );
    for ( std::size_t b = 1; b < bs.size(); ++ b )   /* φ operands need block names */
        if ( !bs[ b ].labelled ) bs[ b ].label = "B" + std::to_string( b ), bs[ b ].labelled = true;

    auto idom = dominators( bs );
    auto df = frontiers( bs, idom );

    /* step 2: φ placement */
    std::map< std::string, std::set< std::size_t > > def_blocks;
    for ( std::size_t b = 0; b < bs.size(); ++ b )
        for ( const auto &i : bs[ b ].code )
            if ( auto d = def( i ) ) def_blocks[ *d ].insert( b );

    std::vector< std::vector< std::string > > phis( bs.size() );   /* the variables needing a φ */
    for ( const auto &[ x, blocks ] : def_blocks )
    {
        std::vector< std::size_t > work( blocks.begin(), blocks.end() );
        std::set< std::size_t > has_phi;
        while ( !work.empty() )
        {
            std::size_t b = work.back();
            work.pop_back();
            for ( auto y : df[ b ] )
                if ( has_phi.insert( y ).second )
                {
                    phis[ y ].push_back( x );
                    work.push_back( y );
                }
        }
    }

    for ( std::size_t b = 0; b < bs.size(); ++ b )
    {
        std::vector< instr > code;
        for ( const auto &x : phis[ b ] ) code.push_back( { "phi", x } );
        code.insert( code.end(), bs[ b ].code.begin(), bs[ b ].code.end() );
        bs[ b ].code = code;
    }

    /* step 3: renaming, in a preorder walk of the dominator tree */
    std::vector< std::vector< std::size_t > > children( bs.size() );
    for ( std::size_t b = 1; b < bs.size(); ++ b )
        if ( idom[ b ] >= 0 ) children[ idom[ b ] ].push_back( b );

    std::map< std::string, std::vector< std::string > > stack;
    std::map< std::string, int > counter;
    auto current = [ & ]( const std::string &x ) { return stack[ x ].empty() ? x + ".0" : stack[ x ].back(); };
    auto rename_use = [ & ]( std::string &s ) { if ( is_var( s ) ) s = current( s ); };

    std::function< void( std::size_t ) > walk = [ & ]( std::size_t b )
    {
        std::vector< std::string > pushed;
        for ( auto &i : bs[ b ].code )
        {
            if ( i.kind != "phi" ) rename_use( i.a ), rename_use( i.b );
            if ( i.kind == "set" || i.kind == "phi" )
            {
                std::string fresh = i.dst + "." + std::to_string( ++ counter[ i.dst ] );
                stack[ i.dst ].push_back( fresh );
                pushed.push_back( i.dst );
                i.dst = fresh;
            }
        }
        for ( auto s : bs[ b ].succs )
        {
            std::size_t k = 0;
            for ( auto &i : bs[ s ].code )
                if ( i.kind == "phi" )
                    i.phi.emplace_back( bs[ b ].label, current( phis[ s ][ k++ ] ) );
        }
        for ( auto c : children[ b ] ) walk( c );
        for ( const auto &x : pushed ) stack[ x ].pop_back();
    };
    walk( 0 );

    return flatten( bs );
}

int main() /* demo */
{
    std::vector< std::string > sum =
    {
        "s = 0", "i = 1",
        "Loop:", "c = i <= 10", "if c goto Body", "goto Done",
        "Body:", "s = s + i", "i = i + 1", "goto Loop",
        "Done:", "print s",
    };

    auto p = parse( sum );
    auto bs = build_cfg( p );
    auto idom = dominators( bs );
    assert(( idom == std::vector< int >{ -1, 0, 1, 1, 2 } ));   /* Entry, Loop, @2, Body, Done */
    auto df = frontiers( bs, idom );
    assert(( df[ 3 ] == std::set< std::size_t >{ 1 } ));        /* Body's definitions meet at Loop */

    /* Notice the φ for ‹c›: ‹c› is assigned in ‹Loop›, whose dominance
     * frontier contains ‹Loop› itself (because of the back edge). The φ
     * is «dead» – ‹c.1› is never used – this is «minimal» SSA; «pruned»
     * SSA would use liveness (‹p1_liveness›) to avoid such φs. */
    auto ssa = to_ssa( p );
    assert( show( ssa ) == ( std::vector< std::string >{
        "s.1 = 0", "i.1 = 1",
        "Loop:", "c.1 = phi Entry:c.0 Body:c.2", "i.2 = phi Entry:i.1 Body:i.3", "s.2 = phi Entry:s.1 Body:s.3",
        "c.2 = i.2 <= 10", "if c.2 goto Body",
        "B2:", "goto Done",
        "Body:", "s.3 = s.2 + i.2", "i.3 = i.2 + 1", "goto Loop",
        "Done:", "print s.2" } ) );
    assert( run_ir( ssa ) == run_ir( p ) );

    /* every variable is assigned exactly once */
    std::set< std::string > assigned;
    for ( const auto &i : ssa )
        if ( auto d = def( i ) ) assert( assigned.insert( *d ).second );

    /* a diamond */
    auto d = parse( { "if a goto T", "x = 1", "goto J", "T:", "x = 2", "J:", "print x" } );
    assert( show( to_ssa( d ) ) == ( std::vector< std::string >{
        "if a.0 goto T", "B1:", "x.1 = 1", "goto J", "T:", "x.2 = 2", "J:", "x.3 = phi B1:x.1 T:x.2", "print x.3" } ) );

    return 0;
}
