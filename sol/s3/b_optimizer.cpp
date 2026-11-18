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

/* In this assignment, you will write an optimizer for the
 * three-address code of Chapter 11 – the middle part of an optimizing
 * compiler. It combines most of the chapter: the CFG, dominators, SSA
 * construction, conditional constant propagation, dead code
 * elimination and the translation out of SSA. The TAC and the CFG
 * builder are given below. */

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

/* Programs have «inputs»: the variables ‹arg0›, ‹arg1›, … are set by
 * ‹run_args› before the program starts (all other variables start as
 * 0, as usual). The optimizer does not know the inputs. */

std::optional< std::vector< long > > run_args( const std::vector< std::string > &p,
                                               const std::vector< long > &args, long fuel = 100000 )
{
    auto code = parse( p );
    std::vector< instr > init;
    for ( std::size_t k = 0; k < args.size(); ++ k )
        init.push_back( { "set", "arg" + std::to_string( k ), std::to_string( args[ k ] ) } );
    if ( !code.empty() && code[ 0 ].kind == "label" )   /* keep ‹Entry› the entry */
        init.push_back( { "goto", "", "", "", "", code[ 0 ].target } );
    code.insert( code.begin(), init.begin(), init.end() );
    return run_ir( code, fuel );
}

/* Write ‹optimize( program )›. The input is a TAC program without ‹phi›
 * instructions; the output must be a TAC program without ‹phi›, which
 * behaves «exactly» like the input for all inputs: it prints the same
 * values, and it fails (division by zero) if and only if the input
 * fails. (You do not need to care about programs which do not
 * terminate, or run out of fuel.) Variable and label names in the
 * output can be anything.
 *
 * The optimizer must perform at least the following – the tests check
 * the result for programs where these are enough:
 *
 *  • «constant propagation» in the strength of ‹11/r1_sccp›: a variable
 *    which is constant on every path the program can take is replaced
 *    by the constant wherever it is used (‹print x› becomes ‹print 5›),
 *    conditional jumps on constants are resolved, and blocks which can
 *    never execute are removed – keep in mind that variables which are
 *    not inputs start as the «constant» 0,
 *  • «copy propagation»: after ‹x = y›, uses of ‹x› read ‹y› instead
 *    (where that is correct – SSA makes it easy),
 *  • «dead code elimination»: an assignment whose value is never used
 *    (directly, or through other assignments which are used) is
 *    removed – except a division or remainder which might fail (its
 *    divisor is not a known non-zero constant),
 *  • «cleanup»: remove a ‹goto› or ‹if› to the label which immediately
 *    follows it, and labels which are not the target of any jump;
 *    an ‹if› on a constant becomes a ‹goto› (or disappears).
 *
 * The recommended approach is: build the CFG (and add an empty entry
 * block if the first block has predecessors), remove unreachable
 * blocks, convert to SSA (giving each variable a version which stands
 * for its value at the entry – its input value, or 0), run SCCP,
 * rewrite the program using its results, propagate copies, remove
 * dead code, translate out of SSA (‹11/r3_outofssa› – mind the lost
 * copies and swaps!) and clean up. */

// std::vector< std::string > optimize( const std::vector< std::string > &program );

/* ── names ── */

struct names
{
    std::set< std::string > used;

    explicit names( const std::vector< instr > &p )
    {
        for ( const auto &i : p )
        {
            if ( auto d = def( i ) ) used.insert( *d );
            for ( const auto &u : uses( i ) ) used.insert( u );
            if ( !i.target.empty() ) used.insert( i.target );
        }
    }

    std::string fresh( const std::string &base )
    {
        std::string n = base;
        for ( int k = 1; used.contains( n ); ++ k ) n = base + std::to_string( k );
        used.insert( n );
        return n;
    }
};

bool is_input( const std::string &x )
{
    return x.size() > 3 && x.starts_with( "arg" ) && x.find_first_not_of( "0123456789", 3 ) == x.npos;
}

std::vector< block > rebuild( const std::vector< block > &bs ) { return build_cfg( flatten( bs ) ); }

/* ── dominators (the simple iterative algorithm) ── */

std::vector< int > immediate_dominators( const std::vector< block > &bs )
{
    std::size_t n = bs.size();
    std::vector< std::set< std::size_t > > dom( n );
    std::set< std::size_t > all;
    for ( std::size_t b = 0; b < n; ++ b ) all.insert( b );
    for ( std::size_t b = 1; b < n; ++ b ) dom[ b ] = all;
    dom[ 0 ] = { 0 };

    for ( bool changed = true; changed; )
    {
        changed = false;
        for ( std::size_t b = 1; b < n; ++ b )
        {
            std::set< std::size_t > d = all;
            for ( auto p : bs[ b ].preds )
            {
                std::set< std::size_t > m;
                std::set_intersection( d.begin(), d.end(), dom[ p ].begin(), dom[ p ].end(),
                                       std::inserter( m, m.end() ) );
                d = m;
            }
            d.insert( b );
            if ( d != dom[ b ] ) dom[ b ] = d, changed = true;
        }
    }

    std::vector< int > idom( n, -1 );
    for ( std::size_t b = 1; b < n; ++ b )
    {
        std::size_t best = 0;
        for ( auto d : dom[ b ] )
            if ( d != b && dom[ d ].size() >= dom[ best ].size() ) best = d;
        idom[ b ] = best;
    }
    return idom;
}

/* ── SSA construction (as in ‹11/d3_ssa›); the value of a variable at
 * the entry is its name for inputs, and the constant 0 otherwise ── */

void to_ssa( std::vector< block > &bs, names &nm )
{
    auto idom = immediate_dominators( bs );
    std::size_t n = bs.size();

    std::vector< std::set< std::size_t > > df( n );
    for ( std::size_t b = 0; b < n; ++ b )
        if ( bs[ b ].preds.size() >= 2 )
            for ( auto p : bs[ b ].preds )
                for ( int r = p; r != -1 && r != idom[ b ]; r = idom[ r ] )
                    df[ r ].insert( b );

    std::map< std::string, std::set< std::size_t > > def_blocks;
    for ( std::size_t b = 0; b < n; ++ b )
        for ( const auto &i : bs[ b ].code )
            if ( auto d = def( i ) ) def_blocks[ *d ].insert( b );

    std::vector< std::vector< std::string > > phis( n );
    for ( const auto &[ x, blocks ] : def_blocks )
    {
        std::vector< std::size_t > work( blocks.begin(), blocks.end() );
        std::set< std::size_t > has;
        while ( !work.empty() )
        {
            auto b = work.back();
            work.pop_back();
            for ( auto y : df[ b ] )
                if ( has.insert( y ).second ) phis[ y ].push_back( x ), work.push_back( y );
        }
    }
    for ( std::size_t b = 0; b < n; ++ b )
    {
        std::vector< instr > code;
        for ( const auto &x : phis[ b ] ) code.push_back( { "phi", x } );
        code.insert( code.end(), bs[ b ].code.begin(), bs[ b ].code.end() );
        bs[ b ].code = code;
    }

    std::vector< std::vector< std::size_t > > children( n );
    for ( std::size_t b = 1; b < n; ++ b ) children[ idom[ b ] ].push_back( b );

    std::map< std::string, std::vector< std::string > > stack;
    auto current = [ & ]( const std::string &x ) -> std::string
    {
        if ( !stack[ x ].empty() ) return stack[ x ].back();
        return is_input( x ) ? x : "0";
    };

    std::function< void( std::size_t ) > walk = [ & ]( std::size_t b )
    {
        std::vector< std::string > pushed;
        for ( auto &i : bs[ b ].code )
        {
            if ( i.kind != "phi" )
                for ( auto *s : { &i.a, &i.b } ) if ( is_var( *s ) ) *s = current( *s );
            if ( auto d = def( i ) )
            {
                std::string v = nm.fresh( *d + "." );
                stack[ *d ].push_back( v );
                pushed.push_back( *d );
                i.dst = v;
            }
        }
        for ( auto s : bs[ b ].succs )
        {
            std::size_t k = 0;
            for ( auto &i : bs[ s ].code )
                if ( i.kind == "phi" ) i.phi.emplace_back( bs[ b ].label, current( phis[ s ][ k++ ] ) );
        }
        for ( auto c : children[ b ] ) walk( c );
        for ( const auto &x : pushed ) stack[ x ].pop_back();
    };
    walk( 0 );
}

/* ── sparse conditional constant propagation (‹11/r1_sccp›) ── */

struct lat
{
    enum kind { top, constant, bottom } k = top;
    long c = 0;
    bool operator==( const lat & ) const = default;
};

lat meet( lat a, lat b )
{
    if ( a.k == lat::top ) return b;
    if ( b.k == lat::top ) return a;
    if ( a == b ) return a;
    return { lat::bottom };
}

std::size_t index_of( const std::vector< block > &bs, const std::string &label )
{
    for ( std::size_t k = 0; k < bs.size(); ++ k ) if ( bs[ k ].label == label ) return k;
    throw std::logic_error( "no such block" );
}

void sccp( std::vector< block > &bs )
{
    std::map< std::string, lat > val;
    auto get = [ & ]( const std::string &s ) -> lat
    {
        if ( is_int( s ) ) return { lat::constant, std::stol( s ) };
        if ( is_input( s ) ) return { lat::bottom };
        return val[ s ];
    };

    std::vector< bool > exec( bs.size() );
    std::set< std::pair< std::size_t, std::size_t > > edges;
    exec[ 0 ] = true;

    for ( bool changed = true; changed; )
    {
        changed = false;
        auto set = [ & ]( const std::string &x, lat v )
        {
            lat m = meet( val[ x ], v );
            if ( !( m == val[ x ] ) ) val[ x ] = m, changed = true;
        };
        auto edge = [ & ]( std::size_t a, std::size_t b )
        {
            if ( edges.insert( { a, b } ).second ) changed = true;
            if ( !exec[ b ] ) exec[ b ] = true, changed = true;
        };

        for ( std::size_t b = 0; b < bs.size(); ++ b )
        {
            if ( !exec[ b ] ) continue;
            for ( const auto &i : bs[ b ].code )
            {
                if ( i.kind == "phi" )
                {
                    lat v;
                    for ( const auto &[ l, x ] : i.phi )
                        if ( edges.contains( { index_of( bs, l ), b } ) ) v = meet( v, get( x ) );
                    set( i.dst, v );
                }
                else if ( i.kind == "set" )
                {
                    lat a = get( i.a ), r = a;
                    if ( !i.op.empty() )
                    {
                        lat c = get( i.b );
                        if ( a.k == lat::bottom || c.k == lat::bottom ) r = { lat::bottom };
                        else if ( a.k == lat::top || c.k == lat::top ) r = {};
                        else if ( ( i.op == "/" || i.op == "%" ) && c.c == 0 ) r = { lat::bottom };
                        else r = { lat::constant, apply_op( i.op, a.c, c.c ) };
                    }
                    set( i.dst, r );
                }
            }
            const auto &code = bs[ b ].code;
            const instr *last = code.empty() ? nullptr : &code.back();
            bool has_next = b + 1 < bs.size();
            if ( last && last->kind == "goto" ) edge( b, index_of( bs, last->target ) );
            else if ( last && last->kind == "if" )
            {
                lat c = get( last->a );
                if ( c.k == lat::bottom || ( c.k == lat::constant && c.c != 0 ) ) edge( b, index_of( bs, last->target ) );
                if ( ( c.k == lat::bottom || ( c.k == lat::constant && c.c == 0 ) ) && has_next ) edge( b, b + 1 );
            }
            else if ( has_next ) edge( b, b + 1 );
        }
    }

    /* rewrite: substitute constants, resolve branches, drop dead blocks */
    auto subst = [ & ]( std::string &s ) { if ( is_var( s ) && val[ s ].k == lat::constant ) s = std::to_string( val[ s ].c ); };
    std::vector< block > out;
    for ( std::size_t b = 0; b < bs.size(); ++ b )
    {
        if ( !exec[ b ] ) continue;
        block nb = bs[ b ];
        nb.code.clear();
        for ( auto i : bs[ b ].code )
        {
            if ( i.kind == "phi" )
            {
                std::erase_if( i.phi, [ & ]( auto &p ) { return !edges.contains( { index_of( bs, p.first ), b } ); } );
                for ( auto &[ _, x ] : i.phi ) subst( x );
            }
            else subst( i.a ), subst( i.b );
            if ( i.kind == "set" && val[ i.dst ].k == lat::constant ) i = { "set", i.dst, std::to_string( val[ i.dst ].c ) };
            if ( i.kind == "if" && is_int( i.a ) )
            {
                if ( std::stol( i.a ) == 0 ) continue;
                i = { "goto", "", "", "", "", i.target };
            }
            nb.code.push_back( i );
        }
        out.push_back( nb );
    }
    bs = rebuild( out );
}

/* ── copy propagation and dead code elimination on SSA ── */

void propagate_copies( std::vector< block > &bs )
{
    std::map< std::string, std::string > copy;
    for ( auto &b : bs )
        for ( auto &i : b.code )
        {
            if ( i.kind == "phi" && i.phi.size() == 1 ) i = { "set", i.dst, i.phi[ 0 ].second };
            /* a φ whose operands are all the same value (or itself) is a copy, too */
            if ( i.kind == "phi" )
            {
                std::set< std::string > ops;
                for ( auto &[ _, x ] : i.phi ) if ( x != i.dst ) ops.insert( x );
                if ( ops.size() == 1 ) i = { "set", i.dst, *ops.begin() };
            }
            if ( i.kind == "set" && i.op.empty() ) copy[ i.dst ] = i.a;
        }

    std::function< std::string( const std::string & ) > resolve = [ & ]( const std::string &x ) -> std::string
    {
        auto it = copy.find( x );
        if ( it == copy.end() || it->second == x ) return x;
        return resolve( it->second );
    };
    for ( auto &b : bs )
    {
        std::erase_if( b.code, [ & ]( const instr &i ) { return i.kind == "set" && copy.contains( i.dst ); } );
        for ( auto &i : b.code )
        {
            for ( auto *s : { &i.a, &i.b } ) if ( is_var( *s ) ) *s = resolve( *s );
            for ( auto &[ _, x ] : i.phi ) if ( is_var( x ) ) x = resolve( x );
        }
    }
}

bool may_fail( const instr &i )
{
    return i.kind == "set" && ( i.op == "/" || i.op == "%" ) && ( !is_int( i.b ) || std::stol( i.b ) == 0 );
}

void eliminate_dead( std::vector< block > &bs )
{
    std::map< std::string, const instr * > defs;
    for ( auto &b : bs ) for ( auto &i : b.code ) if ( auto d = def( i ) ) defs[ *d ] = &i;

    std::set< std::string > live;
    std::vector< const instr * > work;
    for ( auto &b : bs )
        for ( auto &i : b.code )
            if ( i.kind == "print" || i.kind == "if" || may_fail( i ) ) work.push_back( &i );
    while ( !work.empty() )
    {
        const instr *i = work.back();
        work.pop_back();
        for ( const auto &u : uses( *i ) )
            if ( live.insert( u ).second && defs.contains( u ) ) work.push_back( defs[ u ] );
    }
    for ( auto &b : bs )
        std::erase_if( b.code, [ & ]( const instr &i )
                       { auto d = def( i ); return d && !live.contains( *d ) && !may_fail( i ); } );
}

/* ── out of SSA (‹11/r3_outofssa›) ── */

std::vector< instr > sequential( std::vector< std::pair< std::string, std::string > > copies, names &nm )
{
    std::vector< instr > out;
    std::erase_if( copies, []( auto &c ) { return c.first == c.second; } );
    while ( !copies.empty() )
    {
        bool done = false;
        for ( std::size_t k = 0; k < copies.size() && !done; ++ k )
        {
            bool needed = false;
            for ( std::size_t j = 0; j < copies.size(); ++ j )
                if ( j != k && copies[ j ].second == copies[ k ].first ) needed = true;
            if ( needed ) continue;
            out.push_back( { "set", copies[ k ].first, copies[ k ].second } );
            copies.erase( copies.begin() + k );
            done = true;
        }
        if ( !done )
        {
            std::string t = nm.fresh( "tmp" ), src = copies[ 0 ].second;
            out.push_back( { "set", t, src } );
            for ( auto &c : copies ) if ( c.second == src ) c.second = t;
        }
    }
    return out;
}

void out_of_ssa( std::vector< block > &bs, names &nm )
{
    std::map< std::size_t, std::vector< block > > after;
    std::vector< block > at_end;

    for ( std::size_t b = 0; b < bs.size(); ++ b )
    {
        std::vector< instr > phis;
        while ( !bs[ b ].code.empty() && bs[ b ].code.front().kind == "phi" )
            phis.push_back( bs[ b ].code.front() ), bs[ b ].code.erase( bs[ b ].code.begin() );
        if ( phis.empty() ) continue;

        for ( auto p : bs[ b ].preds )
        {
            std::vector< std::pair< std::string, std::string > > copies;
            for ( const auto &phi : phis )
                for ( const auto &[ l, x ] : phi.phi )
                    if ( l == bs[ p ].label ) copies.emplace_back( phi.dst, x );
            auto code = sequential( copies, nm );
            if ( code.empty() ) continue;

            auto &pc = bs[ p ].code;
            bool jumps = !pc.empty() && ( pc.back().kind == "goto" || pc.back().kind == "if" );
            if ( bs[ p ].succs.size() == 1 )
            {
                pc.insert( jumps ? pc.end() - 1 : pc.end(), code.begin(), code.end() );
                continue;
            }
            block split{ nm.fresh( "Split" ), true, code };
            split.code.push_back( { "goto", "", "", "", "", bs[ b ].label } );
            if ( jumps && pc.back().target == bs[ b ].label )
            {
                pc.back().target = split.label;
                at_end.push_back( split );
            }
            else after[ p ].push_back( split );   /* the fall-through edge */
        }
    }

    std::vector< block > out;
    for ( std::size_t b = 0; b < bs.size(); ++ b )
    {
        out.push_back( bs[ b ] );
        for ( auto &s : after[ b ] ) out.push_back( s );
    }
    if ( !at_end.empty() )
    {
        std::string end = nm.fresh( "End" );
        out.back().code.push_back( { "goto", "", "", "", "", end } );
        out.insert( out.end(), at_end.begin(), at_end.end() );
        out.push_back( { end, true, {} } );
    }
    bs = out;
}

/* ── cleanup ── */

std::vector< instr > cleanup( std::vector< instr > p )
{
    for ( bool changed = true; changed; )
    {
        changed = false;
        std::vector< instr > q;

        /* jumps to the label(s) right after them; code after a ‹goto› up
         * to the next label is unreachable */
        for ( std::size_t k = 0; k < p.size(); ++ k )
        {
            const instr &i = p[ k ];
            if ( i.kind == "goto" || i.kind == "if" )
            {
                bool next = false;
                for ( std::size_t j = k + 1; j < p.size() && p[ j ].kind == "label"; ++ j )
                    if ( p[ j ].target == i.target ) next = true;
                if ( next ) { changed = true; continue; }
            }
            q.push_back( i );
            if ( i.kind == "goto" )
                while ( k + 1 < p.size() && p[ k + 1 ].kind != "label" ) ++ k, changed = true;
        }

        std::set< std::string > targets;
        for ( const auto &i : q ) if ( i.kind == "goto" || i.kind == "if" ) targets.insert( i.target );
        p.clear();
        for ( const auto &i : q )
            if ( i.kind == "label" && !targets.contains( i.target ) ) changed = true;
            else p.push_back( i );
    }
    return p;
}

std::vector< std::string > optimize( const std::vector< std::string > &program )
{
    auto code = parse( program );
    names nm( code );
    nm.used.insert( "Entry" );

    auto bs = build_cfg( code );
    if ( !bs[ 0 ].preds.empty() )   /* a fresh entry block */
    {
        code.insert( code.begin(), { "goto", "", "", "", "", bs[ 0 ].label } );
        bs = build_cfg( code );
    }

    /* remove unreachable blocks */
    std::vector< bool > reach( bs.size() );
    std::vector< std::size_t > work{ 0 };
    reach[ 0 ] = true;
    while ( !work.empty() )
    {
        auto b = work.back();
        work.pop_back();
        for ( auto s : bs[ b ].succs ) if ( !reach[ s ] ) reach[ s ] = true, work.push_back( s );
    }
    std::vector< block > live;
    for ( std::size_t b = 0; b < bs.size(); ++ b ) if ( reach[ b ] ) live.push_back( bs[ b ] );
    bs = rebuild( live );

    /* every block gets a label (φ operands refer to them, and a labelled
     * block does not disappear from ‹build_cfg› when it becomes empty) */
    for ( std::size_t b = 0; b < bs.size(); ++ b )
        if ( !bs[ b ].labelled ) bs[ b ].label = nm.fresh( "B" ), bs[ b ].labelled = true;
    bs = rebuild( bs );

    to_ssa( bs, nm );
    sccp( bs );
    propagate_copies( bs );
    eliminate_dead( bs );
    bs = rebuild( bs );
    out_of_ssa( bs, nm );

    return show( cleanup( flatten( bs ) ) );
}

int main()
{
    using v = std::vector< std::string >;
    auto same = []( const v &p, const v &q, std::vector< std::vector< long > > inputs )
    {
        for ( const auto &in : inputs )
            if ( run_args( p, in ) != run_args( q, in ) ) return false;
        return true;
    };
    auto count = []( const v &p, std::string_view what )
    {
        return std::count_if( p.begin(), p.end(), [ & ]( auto &l ) { return l.find( what ) != l.npos; } );
    };

    /* straight-line constants */
    assert(( optimize( { "a = 2", "b = a * 3", "c = b + a", "print c" } ) == v{ "print 8" } ));
    assert(( optimize( { "print x", "x = 3", "print x" } ) == v{ "print 0", "print 3" } ));

    /* a branch on a constant */
    v branch = { "x = 1", "if x goto T", "print 1", "goto E", "T:", "print 2", "E:" };
    assert(( optimize( branch ) == v{ "print 2" } ));

    /* inputs are unknown; copies and dead code */
    v inputs = { "a = arg0", "b = a", "c = b + 1", "d = c * 2", "print c", "e = arg1 / 2" };
    auto o = optimize( inputs );
    assert( same( inputs, o, { { 1, 2 }, { -5, 0 } } ) );
    assert( o.size() == 2 && count( o, "arg0" ) == 1 );

    /* a possibly failing division is kept, even if its result is dead */
    v fails = { "x = 7 / arg0", "print 1" };
    o = optimize( fails );
    assert( same( fails, o, { { 0 }, { 1 } } ) && !run_args( o, { 0 } ) );
    assert(( optimize( { "x = 7 / 2", "y = arg0 % 3", "print 1" } ) == v{ "print 1" } ));

    /* constants through a loop (‹k› is 5 in every iteration), dead ‹t› */
    v loop = { "i = 0", "s = 0", "k = 5",
               "L:", "c = i < arg0", "if c goto B", "goto E",
               "B:", "t = s * k", "s = s + k", "k = 5", "i = i + 1", "goto L",
               "E:", "print s", "print k" };
    o = optimize( loop );
    assert( same( loop, o, { { 0 }, { 1 }, { 10 } } ) );
    assert( count( o, " = 5" ) == 0 && count( o, "* " ) == 0 && count( o, "print 5" ) == 1 );

    /* the branch on ‹y› is decided, the loop is not */
    v cond = { "y = 0", "n = arg0",
               "L:", "if y goto Never", "n = n - 1", "c = 0 < n", "if c goto L",
               "print n", "goto End",
               "Never:", "print 99", "y = 1", "goto L",
               "End:" };
    o = optimize( cond );
    assert( same( cond, o, { { 1 }, { 5 } } ) );
    assert( count( o, "99" ) == 0 && count( o, "if " ) == 1 );

    /* swaps through φ-functions */
    v swap = { "a = 1", "b = 2", "i = 0",
               "L:", "t = a", "a = b", "b = t", "i = i + 1", "c = i < arg0", "if c goto L",
               "print a", "print b" };
    o = optimize( swap );
    assert( same( swap, o, { { 1 }, { 2 }, { 3 } } ) );

    /* the entry block is a loop header */
    v head = { "L:", "i = i + 1", "c = i < arg0", "if c goto L", "print i" };
    o = optimize( head );
    assert( same( head, o, { { 0 }, { 4 } } ) );

    return 0;
}
