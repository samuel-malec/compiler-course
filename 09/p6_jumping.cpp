#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

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

/* The stack machine of this chapter (see ‹d1_vm› for a detailed
 * description). A program is a vector of instructions; each has an
 * opcode and an integer operand, which only some opcodes use:
 *
 *  • ‹push n› – push the constant ‹n›,
 *  • ‹load i› / ‹store i› – push the value of local slot ‹i› / pop a
 *    value into slot ‹i› (slots start as 0),
 *  • ‹add›, ‹sub›, ‹mul›, ‹div›, ‹mod›, ‹lt›, ‹le›, ‹eq›, ‹ne› – pop ‹b›,
 *    pop ‹a›, push ‹a op b› (comparisons give 1 or 0),
 *  • ‹neg›, ‹not› – replace the top ‹a› by ‹-a›, or by 1 if ‹a = 0› and
 *    0 otherwise,
 *  • ‹dup›, ‹pop›, ‹swap› – duplicate / drop / exchange the top values,
 *  • ‹jump n› – continue at instruction ‹n› (an index into the program),
 *  • ‹jz n› – pop a value; if it is zero, continue at instruction ‹n›,
 *  • ‹print› – pop a value and append it to the output,
 *  • ‹halt› – stop (so does running past the last instruction).
 *
 * ‹execute› runs a program and returns the output, or ‹std::nullopt› if
 * the program does something wrong: a stack underflow, a slot outside
 * of ‹0 … slots - 1›, a jump outside of the program, division by zero,
 * or more than ‹fuel› executed instructions. If ‹st› is given, it is
 * filled with statistics about the run. ‹show› writes an instruction
 * as text (e.g. ‹push 3›, ‹add›). */

enum class op
{
    push, load, store, add, sub, mul, div, mod, lt, le, eq, ne,
    neg, not_, dup, pop, swap, jump, jz, print, halt
};

struct instr
{
    op o;
    long arg = 0;
    bool operator==( const instr & ) const = default;
};

using program = std::vector< instr >;

struct stats
{
    std::size_t max_stack = 0;   /* the largest stack depth reached */
    long steps = 0;              /* the number of executed instructions */
};

const char *op_names[] = { "push", "load", "store", "add", "sub", "mul", "div", "mod",
                           "lt", "le", "eq", "ne", "neg", "not", "dup", "pop", "swap",
                           "jump", "jz", "print", "halt" };

bool has_arg( op o ) { return o == op::push || o == op::load || o == op::store || o == op::jump || o == op::jz; }

std::string show( const instr &i )
{
    std::string s = op_names[ int( i.o ) ];
    return has_arg( i.o ) ? s + " " + std::to_string( i.arg ) : s;
}

std::optional< std::vector< long > > execute( const program &p, std::size_t slots = 16,
                                              stats *st = nullptr, long fuel = 1000000 )
{
    std::vector< long > stack, locals( slots, 0 ), out;
    stats dummy;
    stats &s = st ? *st : dummy;
    s = {};

    auto pop = [ & ]( long &v ) { if ( stack.empty() ) return false; v = stack.back(); stack.pop_back(); return true; };

    for ( std::size_t pc = 0; pc < p.size(); )
    {
        if ( ++ s.steps > fuel ) return std::nullopt;
        const instr &i = p[ pc++ ];
        long a, b;

        switch ( i.o )
        {
            case op::push: stack.push_back( i.arg ); break;
            case op::load:
                if ( i.arg < 0 || std::size_t( i.arg ) >= slots ) return std::nullopt;
                stack.push_back( locals[ i.arg ] ); break;
            case op::store:
                if ( i.arg < 0 || std::size_t( i.arg ) >= slots || !pop( a ) ) return std::nullopt;
                locals[ i.arg ] = a; break;
            case op::neg: case op::not_:
                if ( !pop( a ) ) return std::nullopt;
                stack.push_back( i.o == op::neg ? -a : a == 0 ); break;
            case op::dup:
                if ( !pop( a ) ) return std::nullopt;
                stack.push_back( a ), stack.push_back( a ); break;
            case op::pop:
                if ( !pop( a ) ) return std::nullopt;
                break;
            case op::swap:
                if ( !pop( b ) || !pop( a ) ) return std::nullopt;
                stack.push_back( b ), stack.push_back( a ); break;
            case op::jump:
                if ( i.arg < 0 || std::size_t( i.arg ) > p.size() ) return std::nullopt;
                pc = i.arg; break;
            case op::jz:
                if ( i.arg < 0 || std::size_t( i.arg ) > p.size() || !pop( a ) ) return std::nullopt;
                if ( a == 0 ) pc = i.arg;
                break;
            case op::print:
                if ( !pop( a ) ) return std::nullopt;
                out.push_back( a ); break;
            case op::halt: return out;
            default:
                if ( !pop( b ) || !pop( a ) ) return std::nullopt;
                if ( ( i.o == op::div || i.o == op::mod ) && b == 0 ) return std::nullopt;
                switch ( i.o )
                {
                    case op::add: stack.push_back( a + b ); break;
                    case op::sub: stack.push_back( a - b ); break;
                    case op::mul: stack.push_back( a * b ); break;
                    case op::div: stack.push_back( a / b ); break;
                    case op::mod: stack.push_back( a % b ); break;
                    case op::lt:  stack.push_back( a < b ); break;
                    case op::le:  stack.push_back( a <= b ); break;
                    case op::eq:  stack.push_back( a == b ); break;
                    default:      stack.push_back( a != b ); break;
                }
        }

        s.max_stack = std::max( s.max_stack, stack.size() );
    }

    return out;
}

/* Conditions are usually not values that need to be computed – they
 * decide where to «jump». Compiling ‹(and a b)› as ‘compute ‹a›, compute
 * ‹b›, multiply’ is wasteful, and wrong if ‹b› must not be evaluated when
 * ‹a› is false. Instead, compilers translate conditions into «jumping
 * code»: a function ‹cond( c, target, jump_if )› emits code which jumps
 * to ‹target› if ‹c› is ‹jump_if› (and falls through otherwise):
 *
 *  • ‹(and a b)›, jumping if false: ‹a› jumping to ‹target› if false,
 *    then ‹b› jumping to ‹target› if false; jumping if true: ‹a› jumping
 *    to a new label ‹skip› (after the whole condition) if false, then ‹b›
 *    jumping to ‹target› if true – and dually for ‹or›,
 *  • ‹(not c)› – ‹c› with the opposite ‹jump_if›: no instruction at all,
 *  • a comparison – compute it and use ‹jz›; to jump when it is true,
 *    compute the «opposite» comparison instead (‹a < b› is true iff
 *    ‹b <= a› is false; ‹swap› may help),
 *  • ‹#t›, ‹#f› – an unconditional jump, or nothing.
 *
 * Write a compiler for the language of the introduction (with ‹let›),
 * extended with ‹(if COND e₁ e₂)›, where conditions are:
 *
 *     COND = #t | #f | (and COND COND) | (or COND COND) | (not COND)
 *          | (< e e) | (<= e e) | (= e e) | (!= e e) ;
 *
 * The code must leave the value of the expression on the stack. The
 * operands of ‹and› and ‹or› must be evaluated left to right and
 * «short-circuit». The tests check that no ‹not› instruction is used,
 * and that the result of every comparison is immediately consumed by a
 * ‹jz›. You can assume that the input is well formed. */

// std::pair< program, long > compile( std::string_view expr );  // code and slots

int main()
{
    auto run = []( std::string_view e ) -> std::optional< long >
    {
        auto [ code, slots ] = compile( e );
        for ( std::size_t i = 0; i < code.size(); ++ i )
        {
            assert( code[ i ].o != op::not_ );
            op o = code[ i ].o;
            if ( o == op::lt || o == op::le || o == op::eq || o == op::ne )
                assert( i + 1 < code.size() && code[ i + 1 ].o == op::jz );
        }
        code.push_back( { op::print } );
        auto out = execute( code, slots );
        if ( !out ) return std::nullopt;
        return out->at( 0 );
    };

    assert( run( "(if (< 1 2) 10 20)" ) == 10 );
    assert( run( "(if (not (< 1 2)) 10 20)" ) == 20 );
    assert( run( "(if #t 1 2)" ) == 1 );
    assert( run( "(if (not #t) 1 2)" ) == 2 );
    assert( run( "(let x 5 (if (and (< 0 x) (< x 10)) 1 0))" ) == 1 );
    assert( run( "(let x 15 (if (and (< 0 x) (< x 10)) 1 0))" ) == 0 );
    assert( run( "(let x 0 (if (or (= x 0) (= (/ 10 x) 2)) 1 0))" ) == 1 );    /* short-circuit */
    assert( run( "(let x 0 (if (and (!= x 0) (= (/ 10 x) 2)) 1 0))" ) == 0 );
    assert( run( "(let x 5 (if (and (!= x 0) (= (/ 10 x) 2)) 1 0))" ) == 1 );
    assert( run( "(let a 1 (let b 2 (if (not (or (< b a) (and (= a 1) (not (<= a b))))) 7 8)))" ) == 7 );
    assert( run( "(if (or #f (or #f (not (not #t)))) 3 4)" ) == 3 );
    assert( run( "(+ (if (= 1 1) 2 3) (if (!= 1 1) 4 5))" ) == 7 );
    assert( !run( "(let x 0 (if (and (= (/ 10 x) 2) (= x 0)) 1 0))" ) );

    return 0;
}
