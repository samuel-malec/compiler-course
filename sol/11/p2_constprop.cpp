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

/* «Constant propagation» finds variables which hold a known constant
 * at some point, on every path. Write a function ‹constprop› which
 * performs a forward data-flow analysis over the lattice
 *
 *     ⊤ (not reached yet)  ⊒  a constant  ⊒  ⊥ (not a constant)
 *
 * for each variable – at the entry, every variable is ‹⊥› (we treat the
 * initial values as unknown inputs, although the interpreter starts
 * them at 0) – where two different constants meet as ‹⊥›, and blocks
 * which are not reached keep ‹⊤›. Then
 * transform the program, instruction by instruction, using the values
 * at each point:
 *
 *  • every variable «operand» whose value is a known constant is
 *    replaced by the constant,
 *  • ‹x = c₁ op c₂› with two constants becomes ‹x = c› (except for ‹/› and
 *    ‹%› by zero, which stay),
 *  • ‹if c goto L› with a constant ‹c› becomes ‹goto L› (‹c ≠ 0›) or is
 *    removed (‹c = 0›).
 *
 * The analysis is done once, on the original program (do not iterate
 * after removing branches), and the analysis must be precise within a
 * block: ‹x = 1; y = x + 1› gives ‹x = 1; y = 2›. Blocks with ‹⊤› at their
 * start are left unchanged. Return the program via ‹flatten›. */

// std::vector< std::string > constprop( const std::vector< std::string > &program );

/* the lattice value of a variable: a constant, or ⊥ (nullopt) */
using env = std::map< std::string, std::optional< long > >;   /* missing: ⊥ */

std::optional< long > value( const env &e, const std::string &s )
{
    if ( is_int( s ) ) return std::stol( s );
    auto it = e.find( s );
    return it == e.end() ? std::nullopt : it->second;
}

void step( env &e, const instr &i )
{
    if ( i.kind != "set" ) return;
    auto a = value( e, i.a );
    if ( i.op.empty() ) { e[ i.dst ] = a; return; }
    auto b = value( e, i.b );
    if ( a && b && !( ( i.op == "/" || i.op == "%" ) && *b == 0 ) )
        e[ i.dst ] = apply_op( i.op, *a, *b );
    else
        e[ i.dst ] = std::nullopt;
}

env meet( const env &x, const env &y )
{
    env out;
    std::set< std::string > keys;
    for ( auto &[ k, _ ] : x ) keys.insert( k );
    for ( auto &[ k, _ ] : y ) keys.insert( k );
    for ( const auto &k : keys )
    {
        auto a = value( x, k ), b = value( y, k );
        out[ k ] = a && b && *a == *b ? a : std::nullopt;
    }
    return out;
}

std::vector< std::string > constprop( const std::vector< std::string > &program )
{
    auto bs = build_cfg( parse( program ) );
    std::size_t n = bs.size();
    std::vector< std::optional< env > > in( n ), out( n );   /* nullopt = ⊤ */
    in[ 0 ] = env{};

    for ( bool changed = true; changed; )
    {
        changed = false;
        for ( std::size_t b = 0; b < n; ++ b )
        {
            std::optional< env > i = b == 0 ? std::optional< env >( env{} ) : std::nullopt;
            for ( auto p : bs[ b ].preds )
                if ( out[ p ] ) i = i ? meet( *i, *out[ p ] ) : *out[ p ];
            if ( !i ) continue;
            env o = *i;
            for ( const auto &x : bs[ b ].code ) step( o, x );
            if ( !out[ b ] || *out[ b ] != o || !in[ b ] || *in[ b ] != *i )
                changed = true;
            in[ b ] = i, out[ b ] = o;
        }
    }

    for ( std::size_t b = 0; b < n; ++ b )
    {
        if ( !in[ b ] ) continue;
        env e = *in[ b ];
        std::vector< instr > code;
        for ( auto x : bs[ b ].code )
        {
            instr orig = x;
            auto subst = [ & ]( std::string &s ) { if ( is_var( s ) ) if ( auto c = value( e, s ) ) s = std::to_string( *c ); };
            subst( x.a ), subst( x.b );

            if ( x.kind == "set" && !x.op.empty() && is_int( x.a ) && is_int( x.b ) &&
                 !( ( x.op == "/" || x.op == "%" ) && std::stol( x.b ) == 0 ) )
                x = { "set", x.dst, std::to_string( apply_op( x.op, std::stol( x.a ), std::stol( x.b ) ) ) };

            if ( x.kind == "if" && is_int( x.a ) )
            {
                if ( std::stol( x.a ) != 0 ) code.push_back( { "goto", "", "", "", "", x.target } );
            }
            else
                code.push_back( x );
            step( e, orig );
        }
        bs[ b ].code = code;
    }

    return show( flatten( bs ) );
}

int main()
{
    using v = std::vector< std::string >;

    assert(( constprop( { "x = 2", "y = x * 3", "z = y + w", "print z" } ) ==
             v{ "x = 2", "y = 6", "z = 6 + w", "print z" } ));
    assert(( constprop( { "print q" } ) == v{ "print q" } ));

    v diamond = { "if a goto T", "x = 5", "goto J", "T:", "x = 5", "J:", "y = x + 1", "if y goto K", "print 0", "K:", "print y" };
    assert(( constprop( diamond ) ==
             v{ "if a goto T", "x = 5", "goto J", "T:", "x = 5", "J:", "y = 6", "goto K", "print 0", "K:", "print 6" } ));

    v loop = { "i = 0", "k = 7", "Loop:", "c = i < 3", "if c goto Body", "goto Done", "Body:",
               "i = i + 1", "j = k * 2", "goto Loop", "Done:", "print i", "print j" };
    auto out = constprop( loop );
    assert(( out == v{ "i = 0", "k = 7", "Loop:", "c = i < 3", "if c goto Body", "goto Done", "Body:",
                       "i = i + 1", "j = 14", "goto Loop", "Done:", "print i", "print j" } ));
    assert( run( out ) == run( loop ) );

    v merge = { "if a goto T", "x = 1", "goto J", "T:", "x = 2", "J:", "print x" };
    assert( constprop( merge ) == merge );

    return 0;
}
