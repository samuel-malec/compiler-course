#include <cassert>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>

/* In this demo, we will write an evaluator for a part of ‹tiny›:
 * integers, booleans, variables, arithmetic and comparison, ‹let›
 * and ‹if›. We will build the trees by hand (the complete pipeline
 * with a parser is in ‹d2_tiny›).
 *
 * First, the «values». A value is the result of evaluating an
 * expression. Since ‹tiny› is dynamically typed, any expression can,
 * in principle, produce a value of any type, so the type of values is
 * a variant. */

using value = std::variant< long, bool >;

/* Evaluation can fail, for instance when adding a number to a
 * boolean. We report such «run-time errors» by throwing an
 * exception, which is the easiest way to abandon a deeply recursive
 * computation. */

struct runtime_error : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/* The AST, in the style of ‹03/d3_rewrite›. Binary operators are
 * represented by their text (‹"+"›, ‹"<="›, …). */

struct node;
using node_ptr = std::shared_ptr< const node >;

struct literal  { value v; };
struct variable { std::string name; };
struct binary   { std::string op; node_ptr left, right; };
struct let      { std::string name; node_ptr value, body; };
struct cond     { node_ptr test, then, otherwise; };

struct node
{
    std::variant< literal, variable, binary, let, cond > v;
};

node_ptr lit( value v ) { return std::make_shared< node >( literal{ v } ); }
node_ptr var( std::string n ) { return std::make_shared< node >( variable{ n } ); }
node_ptr bin( std::string op, node_ptr l, node_ptr r )
{
    return std::make_shared< node >( binary{ op, l, r } );
}
node_ptr mk_let( std::string n, node_ptr v, node_ptr b )
{
    return std::make_shared< node >( let{ n, v, b } );
}
node_ptr mk_if( node_ptr c, node_ptr t, node_ptr e )
{
    return std::make_shared< node >( cond{ c, t, e } );
}

/* The «environment». We use a chain of frames, each holding a single
 * binding and a pointer to the enclosing frame. Extending the
 * environment (for ‹let›) creates a new frame which points to the old
 * one; the old environment is unchanged, which is exactly what we
 * need – once the body of the ‹let› is evaluated, we continue with
 * the outer environment, in which the new variable does not exist.
 *
 * Lookup walks the chain from the innermost frame outwards, so inner
 * bindings «shadow» outer ones with the same name. */

struct frame;
using env = std::shared_ptr< const frame >;

struct frame
{
    std::string name;
    value val;
    env parent;
};

env extend( env parent, std::string name, value v )
{
    return std::make_shared< frame >( std::move( name ), v, parent );
}

value lookup( const env &e, const std::string &name )
{
    for ( const frame *f = e.get(); f; f = f->parent.get() )
        if ( f->name == name )
            return f->val;
    throw runtime_error( "unbound variable " + name );
}

/* Helpers which check the type of a value. This is where dynamic
 * typing happens: the evaluator looks at the «run-time» type of the
 * value, and complains if it is not what the operator needs. */

long as_int( const value &v )
{
    if ( auto i = std::get_if< long >( &v ) )
        return *i;
    throw runtime_error( "expected an integer" );
}

bool as_bool( const value &v )
{
    if ( auto b = std::get_if< bool >( &v ) )
        return *b;
    throw runtime_error( "expected a boolean" );
}

template< typename... fs > struct overloaded : fs... { using fs::operator()...; };

/* The evaluator. Compare each case with the big-step rules in the
 * introduction: the recursive calls are the premises. */

value eval( const node_ptr &n, const env &e );

value eval_binary( const binary &b, const env &e )
{
    /* ‹and› and ‹or› are special: they only evaluate the right
     * operand if the left one does not decide the result already. */
    if ( b.op == "and" )
        return as_bool( eval( b.left, e ) ) && as_bool( eval( b.right, e ) );
    if ( b.op == "or" )
        return as_bool( eval( b.left, e ) ) || as_bool( eval( b.right, e ) );

    value l = eval( b.left, e ), r = eval( b.right, e );

    /* Equality works for any two values of the same type. */
    if ( b.op == "==" ) return l == r;
    if ( b.op == "!=" ) return l != r;

    long x = as_int( l ), y = as_int( r );

    if ( b.op == "+" ) return x + y;
    if ( b.op == "-" ) return x - y;
    if ( b.op == "*" ) return x * y;
    if ( b.op == "<" ) return x < y;
    if ( b.op == "<=" ) return x <= y;

    if ( b.op == "/" || b.op == "%" )
    {
        if ( y == 0 )
            throw runtime_error( "division by zero" );
        return b.op == "/" ? x / y : x % y;
    }

    throw std::logic_error( "unknown operator " + b.op );
}

value eval( const node_ptr &n, const env &e )
{
    return std::visit( overloaded
    {
        []( const literal &l ) { return l.v; },
        [ & ]( const variable &v ) { return lookup( e, v.name ); },
        [ & ]( const binary &b ) { return eval_binary( b, e ); },
        [ & ]( const let &l )
        {
            value v = eval( l.value, e );
            return eval( l.body, extend( e, l.name, v ) );
        },
        [ & ]( const cond &c )
        {
            /* Only one of the branches is evaluated. */
            return as_bool( eval( c.test, e ) ) ? eval( c.then, e )
                                                : eval( c.otherwise, e );
        }
    }, n->v );
}

std::string error_of( const node_ptr &n )
{
    try { eval( n, nullptr ); }
    catch ( const runtime_error &e ) { return e.what(); }
    return "";
}

int main() /* demo */
{
    /* let x = 6 in let y = 7 in x * y */
    auto answer = mk_let( "x", lit( 6L ),
                          mk_let( "y", lit( 7L ), bin( "*", var( "x" ), var( "y" ) ) ) );
    assert( eval( answer, nullptr ) == value( 42L ) );

    /* Shadowing: let x = 1 in (let x = 2 in x) + x */
    auto shadow = mk_let( "x", lit( 1L ),
                          bin( "+", mk_let( "x", lit( 2L ), var( "x" ) ), var( "x" ) ) );
    assert( eval( shadow, nullptr ) == value( 3L ) );

    /* if 1 < 2 then 10 else 1 / 0 – the else branch is never run */
    auto lazy_if = mk_if( bin( "<", lit( 1L ), lit( 2L ) ), lit( 10L ),
                          bin( "/", lit( 1L ), lit( 0L ) ) );
    assert( eval( lazy_if, nullptr ) == value( 10L ) );

    /* false and (1 / 0 == 1) – short-circuit */
    auto sc = bin( "and", lit( false ), bin( "==", bin( "/", lit( 1L ), lit( 0L ) ),
                                                   lit( 1L ) ) );
    assert( eval( sc, nullptr ) == value( false ) );

    /* equality across types is allowed (and false) */
    assert( eval( bin( "==", lit( 1L ), lit( true ) ), nullptr ) == value( false ) );

    /* the environment can also be supplied from outside */
    auto env0 = extend( nullptr, "z", 5L );
    assert( eval( bin( "%", var( "z" ), lit( 3L ) ), env0 ) == value( 2L ) );

    assert( error_of( bin( "+", lit( 1L ), lit( true ) ) ) == "expected an integer" );
    assert( error_of( var( "q" ) ) == "unbound variable q" );
    assert( error_of( mk_if( lit( 1L ), lit( 1L ), lit( 2L ) ) ) == "expected a boolean" );
    assert( error_of( bin( "/", lit( 1L ), lit( 0L ) ) ) == "division by zero" );
    assert( error_of( mk_let( "x", lit( 1L ), var( "x" ) ) ) == "" );

    return 0;
}
