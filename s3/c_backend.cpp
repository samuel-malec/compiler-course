#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* In this assignment, you will write the back end of a compiler: a
 * code generator from the three-address code of Chapter 11 to a
 * simple «register machine», with «register allocation». The machine
 * has ‹k› registers and an unlimited memory of «slots»; variables which
 * do not fit into registers must be «spilled» to memory, and memory
 * accesses are expensive – the quality of your allocation is measured
 * by the number of memory accesses the program performs. */

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

/* The machine. A program is a list of lines:
 *
 *     L:                  ; a label (the syntax of TAC labels)
 *     li rD, n            ; rD = n
 *     mv rD, rS           ; rD = rS
 *     OP rD, rA, rB       ; rD = rA OP rB, for OP one of
 *                         ; add sub mul div mod lt le eq ne
 *     ld rD, [n]          ; rD = memory slot n (n ≥ 0)
 *     st [n], rS          ; memory slot n = rS
 *     jmp L
 *     bnz rS, L           ; jump if rS is not 0
 *     print rS
 *
 * Registers are ‹r0› … ‹r(k-1)›. All registers start as 0. The «inputs»
 * of the program are in memory: slot ‹i› initially holds the input ‹i›
 * (the TAC variable ‹argi›, see below); all other slots start as 0.
 * ‹run_machine› returns ‹std::nullopt› for a malformed line, a register
 * outside of ‹0 … k - 1›, an unknown label, division by zero, or when
 * it runs out of fuel; otherwise the output, the number of executed
 * instructions, and the number of executed ‹ld› and ‹st› instructions. */

struct machine_result
{
    std::vector< long > output;
    long steps = 0, memory = 0;
};

std::optional< machine_result > run_machine( const std::vector< std::string > &code, int k,
                                             const std::vector< long > &inputs, long fuel = 1000000 )
{
    struct ins { std::string op; long d = 0, a = 0, b = 0; std::string label; };
    std::vector< ins > prog;
    std::map< std::string, std::size_t > labels;

    auto words = []( std::string_view l )
    {
        std::vector< std::string > w;
        std::string cur;
        for ( char c : l )
            if ( c == ' ' || c == ',' ) { if ( !cur.empty() ) w.push_back( cur ), cur.clear(); }
            else cur += c;
        if ( !cur.empty() ) w.push_back( cur );
        return w;
    };
    auto reg = [ & ]( const std::string &s ) -> std::optional< long >
    {
        if ( s.size() < 2 || s[ 0 ] != 'r' || !is_int( s.substr( 1 ) ) || s[ 1 ] == '-' ) return std::nullopt;
        long r = std::stol( s.substr( 1 ) );
        if ( r >= k ) return std::nullopt;
        return r;
    };
    auto slot = [ & ]( const std::string &s ) -> std::optional< long >
    {
        if ( s.size() < 3 || s.front() != '[' || s.back() != ']' ) return std::nullopt;
        std::string n = s.substr( 1, s.size() - 2 );
        if ( !is_int( n ) || n[ 0 ] == '-' ) return std::nullopt;
        return std::stol( n );
    };
    static const std::set< std::string > ops = { "add", "sub", "mul", "div", "mod", "lt", "le", "eq", "ne" };

    for ( const auto &line : code )
    {
        auto w = words( line );
        ins i;
        if ( w.size() == 1 && w[ 0 ].back() == ':' && is_label( w[ 0 ].substr( 0, w[ 0 ].size() - 1 ) ) )
        {
            labels[ w[ 0 ].substr( 0, w[ 0 ].size() - 1 ) ] = prog.size();
            continue;
        }
        if ( w.empty() ) return std::nullopt;
        i.op = w[ 0 ];
        bool ok = false;
        if ( i.op == "li" && w.size() == 3 && is_int( w[ 2 ] ) ) { auto d = reg( w[ 1 ] ); ok = bool( d ); i.d = d.value_or( 0 ); i.a = std::stol( w[ 2 ] ); }
        else if ( i.op == "mv" && w.size() == 3 ) { auto d = reg( w[ 1 ] ), a = reg( w[ 2 ] ); ok = d && a; i.d = d.value_or( 0 ), i.a = a.value_or( 0 ); }
        else if ( ops.contains( i.op ) && w.size() == 4 )
        {
            auto d = reg( w[ 1 ] ), a = reg( w[ 2 ] ), b = reg( w[ 3 ] );
            ok = d && a && b; i.d = d.value_or( 0 ), i.a = a.value_or( 0 ), i.b = b.value_or( 0 );
        }
        else if ( i.op == "ld" && w.size() == 3 ) { auto d = reg( w[ 1 ] ), s = slot( w[ 2 ] ); ok = d && s; i.d = d.value_or( 0 ), i.a = s.value_or( 0 ); }
        else if ( i.op == "st" && w.size() == 3 ) { auto s = slot( w[ 1 ] ), a = reg( w[ 2 ] ); ok = s && a; i.d = s.value_or( 0 ), i.a = a.value_or( 0 ); }
        else if ( i.op == "jmp" && w.size() == 2 && is_label( w[ 1 ] ) ) { ok = true; i.label = w[ 1 ]; }
        else if ( i.op == "bnz" && w.size() == 3 && is_label( w[ 2 ] ) ) { auto a = reg( w[ 1 ] ); ok = bool( a ); i.a = a.value_or( 0 ); i.label = w[ 2 ]; }
        else if ( i.op == "print" && w.size() == 2 ) { auto a = reg( w[ 1 ] ); ok = bool( a ); i.a = a.value_or( 0 ); }
        if ( !ok ) return std::nullopt;
        prog.push_back( i );
    }

    machine_result res;
    std::vector< long > r( k );
    std::map< long, long > mem;
    for ( std::size_t i = 0; i < inputs.size(); ++ i ) mem[ i ] = inputs[ i ];

    for ( std::size_t pc = 0; pc < prog.size(); )
    {
        if ( ++ res.steps > fuel ) return std::nullopt;
        const ins &i = prog[ pc++ ];
        auto jump = [ & ]() -> bool
        {
            auto it = labels.find( i.label );
            if ( it == labels.end() ) return false;
            pc = it->second;
            return true;
        };
        if ( i.op == "li" ) r[ i.d ] = i.a;
        else if ( i.op == "mv" ) r[ i.d ] = r[ i.a ];
        else if ( i.op == "ld" ) r[ i.d ] = mem[ i.a ], ++ res.memory;
        else if ( i.op == "st" ) mem[ i.d ] = r[ i.a ], ++ res.memory;
        else if ( i.op == "jmp" ) { if ( !jump() ) return std::nullopt; }
        else if ( i.op == "bnz" ) { if ( r[ i.a ] && !jump() ) return std::nullopt; }
        else if ( i.op == "print" ) res.output.push_back( r[ i.a ] );
        else
        {
            long x = r[ i.a ], y = r[ i.b ];
            if ( ( i.op == "div" || i.op == "mod" ) && y == 0 ) return std::nullopt;
            static const std::map< std::string, std::string > tac =
                { { "add", "+" }, { "sub", "-" }, { "mul", "*" }, { "div", "/" }, { "mod", "%" },
                  { "lt", "<" }, { "le", "<=" }, { "eq", "==" }, { "ne", "!=" } };
            r[ i.d ] = apply_op( tac.at( i.op ), x, y );
        }
    }
    return res;
}

/* The input of the code generator is a TAC program without ‹phi›. As
 * in ‹b_optimizer›, the variables ‹arg0›, ‹arg1›, … are the inputs of the
 * program (‹run_args› below runs a TAC program with given inputs), all
 * other variables start as 0.
 *
 * Write ‹codegen( program, k )› (with ‹k ≥ 3›), which produces a program
 * for the machine with ‹k› registers which prints the same values as
 * the TAC program, for all inputs, and fails (division by zero) if and
 * only if the TAC program fails. The recommended approach:
 *
 *  1. compute «liveness» on the CFG (‹11/p1_liveness›), and from it the
 *     «live range» of each variable – as an «interval» of positions in
 *     the program (linear scan, Poletto and Sarkar, 1999), or as nodes
 *     of an «interference graph» (graph colouring, ‹11/r2_regalloc›),
 *  2. assign registers to as many variables as possible; the others are
 *     spilled to memory slots (an input which is spilled can simply
 *     stay in its slot); reserve two registers for working with
 *     spilled values,
 *  3. generate code: a spilled operand is loaded into a reserved
 *     register before the instruction, a spilled result is computed
 *     into a reserved register and stored after it; constants are put
 *     into registers with ‹li›; an input with a register is loaded into
 *     it at the start.
 *
 * The tests check the «number of memory accesses» for a few programs
 * with a low «register pressure»: if, at every point of the program,
 * at most ‹k - 2› variables are live, no variable should be spilled (so
 * the only memory accesses are the loads of the inputs, one per input
 * used). Note that variables which are never live at the same time can
 * share a register. */

std::optional< std::vector< long > > run_args( const std::vector< std::string > &p,
                                               const std::vector< long > &args, long fuel = 1000000 )
{
    auto code = parse( p );
    std::vector< instr > init;
    for ( std::size_t k = 0; k < args.size(); ++ k )
        init.push_back( { "set", "arg" + std::to_string( k ), std::to_string( args[ k ] ) } );
    if ( !code.empty() && code[ 0 ].kind == "label" )
        init.push_back( { "goto", "", "", "", "", code[ 0 ].target } );
    code.insert( code.begin(), init.begin(), init.end() );
    return run_ir( code, fuel );
}

// std::vector< std::string > codegen( const std::vector< std::string > &program, int k );

int main()
{
    using v = std::vector< std::string >;
    auto check = []( const v &p, int k, std::vector< std::vector< long > > inputs, long max_memory = -1 )
    {
        auto code = codegen( p, k );
        for ( const auto &in : inputs )
        {
            auto want = run_args( p, in );
            auto got = run_machine( code, k, in );
            if ( !want ) { if ( got ) return false; continue; }
            if ( !got || got->output != *want ) return false;
            if ( max_memory >= 0 && got->memory > max_memory ) return false;
        }
        return true;
    };

    v simple = { "a = arg0 + 1", "b = a * a", "print b", "print 7" };
    assert( check( simple, 3, { { 0 }, { 4 } } ) );
    assert( check( simple, 4, { { 4 } }, 1 ) );

    /* a long chain: many variables, but only two live at a time */
    v chain = { "x0 = arg0" };
    for ( int i = 1; i < 30; ++ i )
        chain.push_back( "x" + std::to_string( i ) + " = x" + std::to_string( i - 1 ) + " + " + std::to_string( i ) );
    chain.push_back( "print x29" );
    assert( check( chain, 4, { { 0 }, { 10 } }, 1 ) );
    assert( check( chain, 3, { { 10 } } ) );

    /* a loop: ‹n›, ‹s›, ‹i› and ‹c› */
    v loop = { "n = arg0", "s = 0", "i = 0",
               "L:", "c = i < n", "if c goto B", "goto E",
               "B:", "s = s + i", "i = i + 1", "goto L",
               "E:", "print s" };
    assert( check( loop, 6, { { 0 }, { 10 }, { 100 } }, 1 ) );
    assert( check( loop, 3, { { 0 }, { 10 } } ) );

    /* high pressure: ten values live at once */
    v wide;
    for ( int i = 0; i < 10; ++ i ) wide.push_back( "v" + std::to_string( i ) + " = arg0 * " + std::to_string( i ) );
    for ( int i = 1; i < 10; ++ i ) wide.push_back( "v0 = v0 + v" + std::to_string( i ) );
    wide.push_back( "print v0" );
    assert( check( wide, 3, { { 1 }, { 3 } } ) );
    assert( check( wide, 5, { { 3 } } ) );
    assert( check( wide, 12, { { 3 } }, 1 ) );

    /* variables which are read before they are written; failures */
    assert( check( { "print z", "z = 5", "print z", "y = 1 / arg1", "print y" }, 3, { { 0, 0 }, { 0, 1 } } ) );
    assert( check( { "L:", "i = i + 1", "c = i < arg0", "if c goto L", "print i" }, 5, { { 0 }, { 5 } }, 1 ) );

    return 0;
}
