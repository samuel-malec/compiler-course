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

/* «Sparse conditional constant propagation» (Wegman and Zadeck) is
 * the constant propagation of choice in SSA-based compilers. It is
 * «optimistic»: it assumes that every SSA variable is ‹⊤› (undefined)
 * and every block unreachable, until proven otherwise, and it is
 * «conditional»: a branch on a constant only makes one successor
 * reachable. Combining the two finds more constants than separate
 * analyses: in
 *
 *     x.1 = 1
 *     if x.1 goto T
 *     …                         ; never executed: y.1 = 2
 *     J: y.3 = phi F:y.1 T:y.2  ; only the T edge is executable
 *
 * ‹y.3› is the constant ‹y.2›.
 *
 * Write a function ‹sccp› for programs in SSA form (each variable is
 * assigned once; ‹phi› at the starts of blocks). The rules:
 *
 *  • initially, only the entry block is «executable»; variables which
 *    are never assigned are ‹⊥› (unknown inputs), all others are ‹⊤›,
 *  • in an executable block, an assignment is evaluated on lattice
 *    values: ‹⊤› operands give ‹⊤›, ‹⊥› operands give ‹⊥›, constants are
 *    folded (division by zero gives ‹⊥›); a ‹phi› is the meet of the
 *    operands which come from «executable» edges (‹⊤› meets anything as
 *    the other value; two different constants meet as ‹⊥›),
 *  • the end of an executable block makes edges executable: a ‹goto›
 *    its target; an ‹if› on a constant non-zero value only the target,
 *    on zero only the next block, on ‹⊥› both (on ‹⊤› none yet); a block
 *    without a jump at the end falls through to the next block,
 *  • iterate until nothing changes.
 *
 * The result lists ‹x = c› for every variable whose final value is a
 * constant, sorted by name, followed by ‹dead L› for every block (in
 * block order) which is not executable. */

// std::vector< std::string > sccp( const std::vector< std::string > &program );

/* lattice values: ⊤ (top), a constant, ⊥ (bottom) */
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
    if ( a.k == lat::constant && b.k == lat::constant && a.c == b.c ) return a;
    return { lat::bottom };
}

std::vector< std::string > sccp( const std::vector< std::string > &program )
{
    auto bs = build_cfg( parse( program ) );
    std::map< std::string, lat > val;
    std::set< std::string > defined;
    for ( const auto &b : bs )
        for ( const auto &i : b.code )
            if ( auto d = def( i ) ) defined.insert( *d );

    auto get = [ & ]( const std::string &s ) -> lat
    {
        if ( is_int( s ) ) return { lat::constant, std::stol( s ) };
        if ( !defined.contains( s ) ) return { lat::bottom };
        return val[ s ];
    };

    std::vector< bool > exec( bs.size(), false );
    std::set< std::pair< std::size_t, std::size_t > > edges;   /* executable edges */
    exec[ 0 ] = true;

    for ( bool changed = true; changed; )
    {
        changed = false;
        auto set = [ & ]( const std::string &x, lat v )
        {
            lat m = meet( val[ x ], v );   /* values only go down */
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
            const auto &code = bs[ b ].code;

            for ( const auto &i : code )
            {
                if ( i.kind == "phi" )
                {
                    lat v;
                    for ( const auto &[ l, x ] : i.phi )
                        for ( auto p : bs[ b ].preds )
                            if ( bs[ p ].label == l && edges.contains( { p, b } ) ) v = meet( v, get( x ) );
                    set( i.dst, v );
                }
                else if ( i.kind == "set" )
                {
                    lat a = get( i.a ), r;
                    if ( i.op.empty() ) r = a;
                    else
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

            const instr *last = code.empty() ? nullptr : &code.back();
            auto target = [ & ]( const std::string &l )
            {
                for ( std::size_t k = 0; k < bs.size(); ++ k ) if ( bs[ k ].label == l ) return k;
                return std::size_t( 0 );
            };
            if ( last && last->kind == "goto" ) edge( b, target( last->target ) );
            else if ( last && last->kind == "if" )
            {
                lat c = get( last->a );
                if ( c.k == lat::bottom || ( c.k == lat::constant && c.c != 0 ) ) edge( b, target( last->target ) );
                if ( ( c.k == lat::bottom || ( c.k == lat::constant && c.c == 0 ) ) && b + 1 < bs.size() ) edge( b, b + 1 );
            }
            else if ( b + 1 < bs.size() ) edge( b, b + 1 );
        }
    }

    std::vector< std::string > out;
    for ( const auto &[ x, v ] : val )
        if ( v.k == lat::constant ) out.push_back( x + " = " + std::to_string( v.c ) );
    for ( std::size_t b = 0; b < bs.size(); ++ b )
        if ( !exec[ b ] ) out.push_back( "dead " + bs[ b ].label );
    return out;
}

int main()
{
    using v = std::vector< std::string >;

    assert(( sccp( { "x.1 = 1", "y.1 = x.1 + 2", "print y.1" } ) == v{ "x.1 = 1", "y.1 = 3" } ));
    assert(( sccp( { "y.1 = a.0 + 1", "print y.1" } ) == v{} ));

    v cond = { "x.1 = 1", "if x.1 goto T", "F:", "y.1 = 2", "goto J", "T:", "y.2 = 3",
               "J:", "y.3 = phi F:y.1 T:y.2", "z.1 = y.3 + 1", "print z.1" };
    assert(( sccp( cond ) == v{ "x.1 = 1", "y.2 = 3", "y.3 = 3", "z.1 = 4", "dead F" } ));

    v unknown = { "if a.0 goto T", "F:", "y.1 = 2", "goto J", "T:", "y.2 = 3",
                  "J:", "y.3 = phi F:y.1 T:y.2", "print y.3" };
    assert(( sccp( unknown ) == v{ "y.1 = 2", "y.2 = 3" } ));

    /* optimism through a loop: ‹x.2› is 5 in every iteration */
    v loop = { "i.1 = 0", "x.1 = 5",
               "L:", "i.2 = phi Entry:i.1 B:i.3", "x.2 = phi Entry:x.1 B:x.3", "c.1 = i.2 < 10", "if c.1 goto B",
               "E:", "print x.2", "goto End",
               "B:", "x.3 = x.2 * 1", "i.3 = i.2 + 1", "goto L",
               "End:" };
    assert(( sccp( loop ) == v{ "i.1 = 0", "x.1 = 5", "x.2 = 5", "x.3 = 5" } ));

    /* a loop which is never entered */
    v never = { "k.1 = 0", "L:", "j.1 = phi Entry:k.1 B:j.2", "if k.1 goto B", "goto E", "B:", "j.2 = j.1 + 1",
                "goto L", "E:", "print j.1" };
    assert(( sccp( never ) == v{ "j.1 = 0", "k.1 = 0", "dead B" } ));

    return 0;
}
