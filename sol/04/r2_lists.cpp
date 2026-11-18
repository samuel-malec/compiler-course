#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <memory>
#include <map>

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

/* Add «lists» to the values of an interpreted language. Lists are
 * immutable, and they are built from «cons cells» – pairs of a «head»
 * (the first element) and a «tail» (the rest of the list) – ending
 * with the empty list. Since lists are immutable, a list can be
 * shared as the tail of any number of other lists: ‹(cons 0 l)›
 * does not need to copy ‹l›. Hence a good representation is a
 * ‹std::shared_ptr› to a ‹const› cell, with ‹nullptr› for the empty
 * list.
 *
 * Values are integers, booleans and lists (of arbitrary values,
 * including other lists). The forms are:
 *
 *  • integer literals, ‹#t›, ‹#f›, ‹nil› (the empty list) and
 *    variables (any other atom),
 *  • ‹(+ a b)›, ‹(- a b)›, ‹(* a b)›, ‹(< a b)› on integers,
 *  • ‹(= a b)› – structural equality of any two values (lists are
 *    equal if they have the same length and equal elements),
 *  • ‹(if c t e)› (‹c› must be a boolean) and ‹(let x v b)›,
 *  • ‹(cons h t)› – ‹t› must be a list,
 *  • ‹(head l)› and ‹(tail l)› – ‹l› must be a non-empty list,
 *  • ‹(null? l)› – true iff ‹l› is the empty list (‹l› must be a list),
 *  • ‹(list e₁ … eₙ)› – a list of the values of ‹e₁› to ‹eₙ› (‹n ≥ 0›),
 *  • ‹(range a b)› – the list of integers from ‹a› (inclusive) to ‹b›
 *    (exclusive), empty if ‹a ≥ b›,
 *  • ‹(length l)›, ‹(sum l)› (all elements must be integers),
 *    ‹(reverse l)› and ‹(append l₁ l₂)›.
 *
 * Write a function ‹eval› which returns the value of a program,
 * formatted as a string: integers in decimal, booleans as ‹#t› and
 * ‹#f›, lists as their elements separated by spaces and enclosed in
 * parentheses (‹(1 (2 3) #t)›, ‹()›). Return ‹std::nullopt› if the
 * program is malformed or fails. ‹append› must not copy its second
 * argument. */

// std::optional< std::string > eval( std::string_view program );

struct cell;
using list = std::shared_ptr< const cell >;
using value = std::variant< long, bool, list >;

struct cell
{
    value head;
    list tail;
};

struct eval_error {};

list cons( value h, list t ) { return std::make_shared< const cell >( std::move( h ), std::move( t ) ); }

template< typename T >
const T &as( const value &v )
{
    if ( auto p = std::get_if< T >( &v ) )
        return *p;
    throw eval_error();
}

bool equal( const value &a, const value &b )
{
    if ( a.index() != b.index() )
        return false;
    if ( a.index() != 2 )
        return a == b;

    const cell *x = std::get< list >( a ).get(), *y = std::get< list >( b ).get();
    for ( ; x && y; x = x->tail.get(), y = y->tail.get() )
        if ( x != y && !equal( x->head, y->head ) )
            return false;
        else if ( x == y )
            return true; /* shared tails are equal */
    return !x && !y;
}

/* Build a list from a vector, back to front. */
list from_vector( const std::vector< value > &xs, list tail = nullptr )
{
    for ( auto it = xs.rbegin(); it != xs.rend(); ++ it )
        tail = cons( *it, tail );
    return tail;
}

std::vector< value > to_vector( const list &l )
{
    std::vector< value > out;
    for ( const cell *c = l.get(); c; c = c->tail.get() )
        out.push_back( c->head );
    return out;
}

bool is_int( const std::string &a )
{
    std::size_t start = a[ 0 ] == '-' ? 1 : 0;
    return a.size() > start && a.find_first_not_of( "0123456789", start ) == a.npos;
}

using env_t = std::map< std::string, value >;

value eval( const sexp &e, const env_t &env )
{
    if ( e.is_atom() )
    {
        const auto &a = e.atom;
        if ( is_int( a ) ) return std::stol( a );
        if ( a == "#t" ) return true;
        if ( a == "#f" ) return false;
        if ( a == "nil" ) return list();
        if ( auto it = env.find( a ); it != env.end() ) return it->second;
        throw eval_error();
    }

    const auto &it = e.items;
    if ( it.empty() || !it[ 0 ].is_atom() )
        throw eval_error();

    const std::string &op = it[ 0 ].atom;
    auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) throw eval_error(); };
    auto arg = [ & ]( std::size_t i ) { return eval( it[ i ], env ); };

    if ( op == "list" )
    {
        std::vector< value > xs;
        for ( std::size_t i = 1; i < it.size(); ++ i )
            xs.push_back( arg( i ) );
        return from_vector( xs );
    }

    if ( op == "if" )
    {
        arity( 3 );
        return as< bool >( arg( 1 ) ) ? arg( 2 ) : arg( 3 );
    }

    if ( op == "let" )
    {
        arity( 3 );
        if ( !it[ 1 ].is_atom() )
            throw eval_error();
        env_t inner = env;
        inner.insert_or_assign( it[ 1 ].atom, arg( 2 ) );
        return eval( it[ 3 ], inner );
    }

    if ( op == "head" || op == "tail" || op == "null?" || op == "length" ||
         op == "sum" || op == "reverse" )
    {
        arity( 1 );
        list l = as< list >( arg( 1 ) );

        if ( op == "null?" ) return !l;
        if ( op == "head" || op == "tail" )
        {
            if ( !l ) throw eval_error();
            return op == "head" ? l->head : value( l->tail );
        }

        long n = 0, s = 0;
        list r;
        for ( const cell *c = l.get(); c; c = c->tail.get() )
        {
            ++ n;
            if ( op == "sum" ) s += as< long >( c->head );
            r = cons( c->head, r );
        }
        return op == "length" ? value( n ) : op == "sum" ? value( s ) : value( r );
    }

    arity( 2 );
    value a = arg( 1 ), b = arg( 2 );

    if ( op == "=" ) return equal( a, b );
    if ( op == "cons" ) return cons( a, as< list >( b ) );
    if ( op == "append" )
    {
        const list &second = as< list >( b );
        return from_vector( to_vector( as< list >( a ) ), second );
    }

    long x = as< long >( a ), y = as< long >( b );
    if ( op == "+" ) return x + y;
    if ( op == "-" ) return x - y;
    if ( op == "*" ) return x * y;
    if ( op == "<" ) return x < y;
    if ( op == "range" )
    {
        list r;
        for ( long i = y - 1; i >= x; -- i )
            r = cons( i, r );
        return r;
    }
    throw eval_error();
}

std::string show( const value &v )
{
    if ( auto i = std::get_if< long >( &v ) ) return std::to_string( *i );
    if ( auto b = std::get_if< bool >( &v ) ) return *b ? "#t" : "#f";

    std::string out;
    for ( const auto &x : to_vector( std::get< list >( v ) ) )
        out += ( out.empty() ? "" : " " ) + show( x );
    return "(" + out + ")";
}

std::optional< std::string > eval( std::string_view program )
{
    try
    {
        return show( eval( read( program ), env_t{} ) );
    }
    catch ( const eval_error & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    assert( eval( "nil" ) == "()" );
    assert( eval( "(cons 1 nil)" ) == "(1)" );
    assert( eval( "(list 1 (list 2 3) #t)" ) == "(1 (2 3) #t)" );
    assert( eval( "(list)" ) == "()" );
    assert( eval( "(head (list 4 5))" ) == "4" );
    assert( eval( "(tail (list 4 5))" ) == "(5)" );
    assert( eval( "(null? (tail (list 4)))" ) == "#t" );
    assert( eval( "(range 0 5)" ) == "(0 1 2 3 4)" );
    assert( eval( "(range 3 1)" ) == "()" );
    assert( eval( "(length (range 0 1000))" ) == "1000" );
    assert( eval( "(sum (range 1 101))" ) == "5050" );
    assert( eval( "(reverse (range 0 4))" ) == "(3 2 1 0)" );
    assert( eval( "(append (list 1 2) (list 3))" ) == "(1 2 3)" );
    assert( eval( "(append nil nil)" ) == "()" );
    assert( eval( "(= (list 1 (list 2)) (cons 1 (cons (list 2) nil)))" ) == "#t" );
    assert( eval( "(= (list 1 2) (list 1))" ) == "#f" );
    assert( eval( "(= 1 (list 1))" ) == "#f" );
    assert( eval( "(let l (range 0 3) (list (cons 9 l) (cons 8 l)))" ) ==
            "((9 0 1 2) (8 0 1 2))" );
    assert( eval( "(if (null? nil) (list nil) 0)" ) == "(())" );

    assert( !eval( "(head nil)" ) );
    assert( !eval( "(tail nil)" ) );
    assert( !eval( "(cons 1 2)" ) );
    assert( !eval( "(sum (list 1 #t))" ) );
    assert( !eval( "(null? 1)" ) );
    assert( !eval( "(+ 1 nil)" ) );
    assert( !eval( "(if 1 2 3)" ) );
    assert( !eval( "(append (list 1) 2)" ) );
    assert( !eval( "(length)" ) );

    return 0;
}
