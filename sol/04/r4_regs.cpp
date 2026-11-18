#include <cassert>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <sstream>

/* Tree-walking is not the only way to interpret programs. In block 3,
 * we will compile programs into flat sequences of instructions for a
 * «virtual machine», which executes them one by one, using «jumps»
 * for control flow. As a preview, write an interpreter for a simple
 * «register machine». A program is a list of lines, each of which is
 * either empty, a «label» (a name followed by ‹:›, e.g. ‹loop:›), or
 * an «instruction»:
 *
 *  • ‹set r a› – store the value of ‹a› into register ‹r›,
 *  • ‹add r a b›, ‹sub r a b›, ‹mul r a b› – compute ‹a + b› (etc.)
 *    and store the result into ‹r›,
 *  • ‹jmp L› – continue with the first instruction after label ‹L›,
 *  • ‹jz a L›, ‹jnz a L› – jump to ‹L› if ‹a› is zero (non-zero),
 *    otherwise continue with the next instruction,
 *  • ‹out a› – append the value of ‹a› to the output,
 *  • ‹halt› – stop.
 *
 * Here, ‹r› is a register name (a non-empty sequence of lowercase
 * letters), and ‹a› and ‹b› are «operands»: either a register name or
 * an integer literal (with an optional ‹-›). All registers start at
 * 0. Words in a line are separated by one or more spaces (there may
 * also be spaces at the start or end of a line). The program stops
 * when it executes ‹halt›, or when it runs past the last line.
 *
 * Write a function ‹run( program, fuel )› which executes at most
 * ‹fuel› instructions (labels and empty lines do not count). If the
 * program stops before running out of fuel, the result has ‹halted›
 * set to ‹true›; otherwise, it is ‹false›. In both cases, ‹output›
 * contains the values printed so far.
 *
 * Before running anything, the program must be checked: if any line
 * is not a valid label or instruction (unknown instruction, wrong
 * number or kind of operands), if any jump refers to a label which
 * does not exist, or if a label is defined more than once, return
 * ‹std::nullopt›. */

struct result
{
    std::vector< long > output;
    bool halted;
};

// std::optional< result > run( const std::vector< std::string > &program, long fuel );

/* Instructions are parsed and checked up front into this form; an
 * operand is either a register name or a literal. */

struct operand
{
    bool is_reg;
    std::string reg;
    long lit;
};

struct instr
{
    std::string op;
    std::string target;               /* register or label */
    std::vector< operand > args;
};

std::optional< result > run( const std::vector< std::string > &program, long fuel )
{
    std::vector< instr > code;
    std::map< std::string, std::size_t > labels;

    auto is_reg = []( const std::string &w )
    {
        return !w.empty() && w.find_first_not_of( "abcdefghijklmnopqrstuvwxyz" ) == w.npos;
    };

    auto parse_operand = [ & ]( const std::string &w ) -> std::optional< operand >
    {
        if ( is_reg( w ) )
            return operand{ true, w, 0 };
        std::size_t start = !w.empty() && w[ 0 ] == '-' ? 1 : 0;
        if ( w.size() > start && w.find_first_not_of( "0123456789", start ) == w.npos )
            return operand{ false, "", std::stol( w ) };
        return std::nullopt;
    };

    /* pass 1: parse */
    std::vector< std::string > jumps;
    for ( const auto &line : program )
    {
        std::istringstream in( line );
        std::vector< std::string > w;
        for ( std::string word; in >> word; )
            w.push_back( word );

        if ( w.empty() )
            continue;

        if ( w.size() == 1 && w[ 0 ].size() > 1 && w[ 0 ].back() == ':' )
        {
            auto name = w[ 0 ].substr( 0, w[ 0 ].size() - 1 );
            if ( !is_reg( name ) || !labels.emplace( name, code.size() ).second )
                return std::nullopt;
            continue;
        }

        const std::string &op = w[ 0 ];
        instr i{ op, "", {} };

        /* shape: does it start with a register / label, and how many
         * operands follow? */
        std::size_t operands;
        bool has_target = true, target_is_label = false;

        if ( op == "set" ) operands = 1;
        else if ( op == "add" || op == "sub" || op == "mul" ) operands = 2;
        else if ( op == "jmp" ) operands = 0, target_is_label = true;
        else if ( op == "jz" || op == "jnz" ) operands = 1, target_is_label = true;
        else if ( op == "out" ) operands = 1, has_target = false;
        else if ( op == "halt" ) operands = 0, has_target = false;
        else return std::nullopt;

        if ( w.size() != 1 + has_target + operands )
            return std::nullopt;

        std::size_t k = 1;
        if ( has_target && !target_is_label )
        {
            if ( !is_reg( w[ 1 ] ) )
                return std::nullopt;
            i.target = w[ k++ ];
        }

        for ( std::size_t n = 0; n < operands; ++ n )
        {
            auto o = parse_operand( w[ k++ ] );
            if ( !o )
                return std::nullopt;
            i.args.push_back( *o );
        }

        if ( target_is_label )
        {
            i.target = w[ k ];
            jumps.push_back( i.target );
        }

        code.push_back( i );
    }

    /* pass 2: check jump targets */
    for ( const auto &l : jumps )
        if ( !labels.contains( l ) )
            return std::nullopt;

    /* execution */
    std::map< std::string, long > regs;
    result res{ {}, false };
    auto val = [ & ]( const operand &o ) { return o.is_reg ? regs[ o.reg ] : o.lit; };

    for ( std::size_t pc = 0; ; )
    {
        if ( pc >= code.size() )
            return res.halted = true, res;
        if ( fuel-- <= 0 )
            return res;

        const instr &i = code[ pc++ ];

        if ( i.op == "set" ) regs[ i.target ] = val( i.args[ 0 ] );
        else if ( i.op == "add" ) regs[ i.target ] = val( i.args[ 0 ] ) + val( i.args[ 1 ] );
        else if ( i.op == "sub" ) regs[ i.target ] = val( i.args[ 0 ] ) - val( i.args[ 1 ] );
        else if ( i.op == "mul" ) regs[ i.target ] = val( i.args[ 0 ] ) * val( i.args[ 1 ] );
        else if ( i.op == "out" ) res.output.push_back( val( i.args[ 0 ] ) );
        else if ( i.op == "halt" ) return res.halted = true, res;
        else if ( i.op == "jmp" ||
                  ( i.op == "jz" && val( i.args[ 0 ] ) == 0 ) ||
                  ( i.op == "jnz" && val( i.args[ 0 ] ) != 0 ) )
            pc = labels.at( i.target );
    }
}

int main()
{
    using v = std::vector< long >;

    auto countdown = std::vector< std::string >
    {
        "  set n 3",
        "loop:",
        "  out n",
        "  sub n n 1",
        "  jnz n loop",
        "  out 100",
    };

    auto r = run( countdown, 1000 );
    assert( r && r->halted );
    assert(( r->output == v{ 3, 2, 1, 100 } ));

    r = run( countdown, 5 ); /* set, out, sub, jnz, out */
    assert( r && !r->halted );
    assert(( r->output == v{ 3, 2 } ));

    auto fib = std::vector< std::string >
    {
        "set a 0", "set b 1", "set i 10",
        "next:", "jz i done", "out a",
        "add t a b", "set a b", "set b t", "sub i i 1", "jmp next",
        "done:", "halt", "out -1",
    };

    r = run( fib, 1000 );
    assert( r && r->halted );
    assert(( r->output == v{ 0, 1, 1, 2, 3, 5, 8, 13, 21, 34 } ));

    auto forever = std::vector< std::string >{ "top:", "add x x 1", "jmp top" };
    r = run( forever, 1001 );
    assert( r && !r->halted && r->output.empty() );

    assert( run( {}, 0 )->halted );
    assert( run( { "", "   " }, 0 )->halted );
    assert( !run( { "halt" }, 0 )->halted );
    assert(( run( { "mul x -3 7", "out x" }, 10 )->output == v{ -21 } ));

    assert( !run( { "jmp nowhere" }, 10 ) );
    assert( !run( { "a:", "a:" }, 10 ) );
    assert( !run( { "set 1 2" }, 10 ) );
    assert( !run( { "add x 1" }, 10 ) );
    assert( !run( { "div x 1 2" }, 10 ) );
    assert( !run( { "out" }, 10 ) );
    assert( !run( { "out X" }, 10 ) );
    assert( !run( { "label: halt" }, 10 ) );
    assert( !run( { "halt", "jz x" }, 10 ) );

    return 0;
}
