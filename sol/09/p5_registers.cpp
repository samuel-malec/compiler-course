#include <algorithm>
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

/* On a «register machine», instructions name their operands. Write a
 * compiler from arithmetic expressions – integer literals, variables
 * (lowercase names) and ‹(op a b)› with ‹op› one of ‹+ - *› – to code
 * for a machine with registers ‹r0›, ‹r1›, … and three kinds of
 * instructions (written as strings, with single spaces):
 *
 *  • ‹rK = N› – load a constant, ‹rK = x› – load a variable,
 *  • ‹rK = rI op rJ› – compute ‹op›.
 *
 * The value of the expression must end up in ‹r0›, and the code must
 * use as «few registers» as possible (the registers used must be ‹r0›
 * to ‹r(n-1)›). The classic solution is due to Sethi and Ullman: label
 * each node with the number of registers needed to evaluate it – 1 for
 * a leaf; for an operation whose children need ‹l› and ‹r›, ‹max(l, r)›
 * if they differ and ‹l + 1› if they are equal – and evaluate the child
 * which needs «more» registers first (keeping its result while the
 * other child is evaluated). Mind the order of operands for ‹-›. */

// std::vector< std::string > compile( std::string_view expr );

/* The register machine, for the tests: returns ‹r0› and the number of
 * registers used. */
std::pair< long, int > run( const std::vector< std::string > &code, const std::map< std::string, long > &env )
{
    std::map< int, long > reg;
    int used = 0;
    auto r = [ & ]( const std::string &s ) { int k = std::stoi( s.substr( 1 ) ); used = std::max( used, k + 1 ); return k; };
    for ( const auto &line : code )
    {
        std::istringstream in( line );
        std::string dst, eq, a, o, b;
        in >> dst >> eq >> a;
        int d = r( dst );
        long va = a[ 0 ] == 'r' ? reg.at( r( a ) ) : std::isdigit( a[ 0 ] ) || a[ 0 ] == '-' ? std::stol( a ) : env.at( a );
        if ( in >> o >> b )
        {
            long vb = reg.at( r( b ) );
            va = o == "+" ? va + vb : o == "-" ? va - vb : va * vb;
        }
        reg[ d ] = va;
    }
    return { reg.at( 0 ), used };
}

int need( const sexp &e )
{
    if ( e.is_atom() ) return 1;
    int l = need( e.items[ 1 ] ), r = need( e.items[ 2 ] );
    return l == r ? l + 1 : std::max( l, r );
}

void gen( const sexp &e, int base, std::vector< std::string > &out )
{
    std::string rb = "r" + std::to_string( base ), rn = "r" + std::to_string( base + 1 );
    if ( e.is_atom() )
        return out.push_back( rb + " = " + e.atom );

    const sexp &l = e.items[ 1 ], &r = e.items[ 2 ];
    const std::string &o = e.items[ 0 ].atom;
    if ( need( l ) >= need( r ) )
    {
        gen( l, base, out );
        gen( r, base + 1, out );
        out.push_back( rb + " = " + rb + " " + o + " " + rn );
    }
    else
    {
        gen( r, base, out );
        gen( l, base + 1, out );
        out.push_back( rb + " = " + rn + " " + o + " " + rb );
    }
}

std::vector< std::string > compile( std::string_view expr )
{
    std::vector< std::string > out;
    gen( read( expr ), 0, out );
    return out;
}

int main()
{
    std::map< std::string, long > env{ { "a", 1 }, { "b", 2 }, { "c", 3 }, { "d", 4 } };
    auto check = [ & ]( std::string_view e, long value, int regs )
    {
        auto [ v, used ] = run( compile( e ), env );
        return v == value && used == regs;
    };

    assert(( compile( "7" ) == std::vector< std::string >{ "r0 = 7" } ));
    assert(( compile( "(- a b)" ) == std::vector< std::string >{ "r0 = a", "r1 = b", "r0 = r0 - r1" } ));
    assert( check( "(+ a (* b c))", 7, 2 ) );
    assert( check( "(- (* a b) (* c d))", -10, 3 ) );
    assert( check( "(- a (- b (- c d)))", -2, 2 ) );
    assert( check( "(- (- (- a b) c) d)", -8, 2 ) );
    assert( check( "(- (+ a b) (- (* c d) (+ a 10)))", 2, 3 ) );
    assert( check( "(* (+ (+ a b) (+ c d)) (+ (+ a b) (+ c d)))", 100, 4 ) );
    assert( check( "(- 1 (* (+ a b) (+ c d)))", -20, 3 ) );

    return 0;
}
