#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <variant>

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

/* Write an interpreter for a small dialect of Lisp (close to Scheme).
 * A program is a «sequence» of forms (you can read it by wrapping the
 * whole text in parentheses before passing it to ‹read›). Values are
 * integers, booleans, «symbols», the empty list, «pairs» (cons cells)
 * and procedures. The forms:
 *
 *  • ‹(define x e)› – at the top level only: bind (or re-bind) the
 *    global variable ‹x›; its value as a form is the «symbol» ‹x›,
 *  • integer literals, ‹#t›, ‹#f›, ‹nil› (the empty list), variables,
 *  • ‹(quote d)› – the datum ‹d› as a value: numbers are integers, other
 *    atoms are symbols (except ‹#t› and ‹#f›), and lists are lists,
 *  • ‹(lambda (x …) e₁ e₂ …)› – a procedure; the body is one or more
 *    expressions, evaluated in order (the value is that of the last),
 *  • ‹(if c t e)› – everything except ‹#f› counts as true,
 *  • ‹(let ((x e) …) b₁ b₂ …)› – all ‹e›s are evaluated in the outer
 *    environment, then the body as with ‹lambda›,
 *  • ‹(begin e₁ e₂ …)› – sequence,
 *  • a call ‹(f a …)›: ‹f› evaluates to a procedure, which is either a
 *    ‹lambda› or one of the «built-in» procedures, which are ordinary
 *    values bound to global variables:
 *    ◦ ‹+›, ‹-›, ‹*› and ‹<› of two integers, ‹=› of two integers,
 *    ◦ ‹cons›, ‹car›, ‹cdr›, ‹null?› (true for the empty list only),
 *      ‹pair?›, ‹not› (true for ‹#f› only), ‹list› (any number of
 *      arguments),
 *    ◦ ‹eq?› – true if both are the same integer, boolean or symbol,
 *      both are the empty list, or both are the same pair (identity).
 *
 * Global variables are looked up when they are used, so a ‹lambda›
 * may refer to a global which is only defined later (this is how
 * recursion and mutual recursion work). Local variables follow
 * lexical scoping, as usual.
 *
 * The result of ‹run› is the value of the last form of the program,
 * written as follows: integers in decimal, ‹#t› and ‹#f›, symbols as
 * their names, procedures as ‹<procedure>›, and lists as in Scheme:
 * ‹(1 2 3)›, ‹()›, and with a dotted tail if the list does not end with
 * the empty list: ‹(1 2 . 3)›. Return ‹std::nullopt› for malformed
 * programs, and for run-time errors (unbound variables, wrong types
 * or numbers of arguments, ‹car› of the empty list, …). */

// std::optional< std::string > run( std::string_view program );

struct bad {};

struct pair;
struct lambda;
struct builtin;
struct nil_t { bool operator==( const nil_t & ) const = default; };
struct symbol { std::string name; };

using value = std::variant< long, bool, symbol, nil_t, const pair *, const lambda *, const builtin * >;

struct pair { value car, cdr; };
struct frame { std::map< std::string, value > vars; frame *parent; };
struct lambda { const sexp *params; const std::vector< sexp > *body; std::size_t first; frame *env; };
struct builtin { std::function< value( std::vector< value > & ) > fn; };

bool is_number( const std::string &a )
{
    std::size_t s = !a.empty() && a[ 0 ] == '-' ? 1 : 0;
    return a.size() > s && a.find_first_not_of( "0123456789", s ) == a.npos;
}

struct lisp
{
    std::vector< std::unique_ptr< pair > > pairs = {};
    std::vector< std::unique_ptr< frame > > frames = {};
    std::vector< std::unique_ptr< lambda > > lambdas = {};
    std::vector< std::unique_ptr< builtin > > builtins = {};
    frame *global = nullptr;

    value cons( value a, value b )
    {
        pairs.push_back( std::make_unique< pair >( a, b ) );
        return pairs.back().get();
    }

    frame *new_frame( frame *parent )
    {
        frames.push_back( std::make_unique< frame >( frame{ {}, parent } ) );
        return frames.back().get();
    }

    static long num( const value &v )
    {
        if ( auto i = std::get_if< long >( &v ) ) return *i;
        throw bad();
    }

    static const pair &as_pair( const value &v )
    {
        if ( auto p = std::get_if< const pair * >( &v ) ) return **p;
        throw bad();
    }

    static bool truthy( const value &v )
    {
        auto b = std::get_if< bool >( &v );
        return !b || *b;
    }

    void define_builtin( const std::string &name, int arity,
                         std::function< value( std::vector< value > & ) > f )
    {
        auto checked = [ = ]( std::vector< value > &args ) -> value
        {
            if ( arity >= 0 && int( args.size() ) != arity ) throw bad();
            return f( args );
        };
        builtins.push_back( std::make_unique< builtin >( checked ) );
        global->vars[ name ] = builtins.back().get();
    }

    lisp()
    {
        global = new_frame( nullptr );
        using a = std::vector< value > &;
        define_builtin( "+", 2, []( a x ) -> value { return num( x[ 0 ] ) + num( x[ 1 ] ); } );
        define_builtin( "-", 2, []( a x ) -> value { return num( x[ 0 ] ) - num( x[ 1 ] ); } );
        define_builtin( "*", 2, []( a x ) -> value { return num( x[ 0 ] ) * num( x[ 1 ] ); } );
        define_builtin( "<", 2, []( a x ) -> value { return num( x[ 0 ] ) < num( x[ 1 ] ); } );
        define_builtin( "=", 2, []( a x ) -> value { return num( x[ 0 ] ) == num( x[ 1 ] ); } );
        define_builtin( "cons", 2, [ this ]( a x ) { return cons( x[ 0 ], x[ 1 ] ); } );
        define_builtin( "car", 1, []( a x ) { return as_pair( x[ 0 ] ).car; } );
        define_builtin( "cdr", 1, []( a x ) { return as_pair( x[ 0 ] ).cdr; } );
        define_builtin( "null?", 1, []( a x ) -> value { return std::holds_alternative< nil_t >( x[ 0 ] ); } );
        define_builtin( "pair?", 1, []( a x ) -> value { return std::holds_alternative< const pair * >( x[ 0 ] ); } );
        define_builtin( "not", 1, []( a x ) -> value { return !truthy( x[ 0 ] ); } );
        define_builtin( "list", -1, [ this ]( a x )
        {
            value l = nil_t{};
            for ( auto it = x.rbegin(); it != x.rend(); ++ it )
                l = cons( *it, l );
            return l;
        } );
        define_builtin( "eq?", 2, []( a x ) -> value
        {
            const value &p = x[ 0 ], &q = x[ 1 ];
            if ( p.index() != q.index() ) return false;
            if ( auto s = std::get_if< symbol >( &p ) ) return s->name == std::get< symbol >( q ).name;
            if ( auto i = std::get_if< long >( &p ) ) return *i == std::get< long >( q );
            if ( auto b = std::get_if< bool >( &p ) ) return *b == std::get< bool >( q );
            if ( std::holds_alternative< nil_t >( p ) ) return true;
            if ( auto pp = std::get_if< const pair * >( &p ) ) return *pp == std::get< const pair * >( q );
            return false;
        } );
    }

    value quote( const sexp &d )
    {
        if ( d.is_atom() )
        {
            if ( is_number( d.atom ) ) return std::stol( d.atom );
            if ( d.atom == "#t" || d.atom == "#f" ) return d.atom == "#t";
            return symbol{ d.atom };
        }
        value l = nil_t{};
        for ( auto it = d.items.rbegin(); it != d.items.rend(); ++ it )
            l = cons( quote( *it ), l );
        return l;
    }

    value body( const std::vector< sexp > &forms, std::size_t first, frame *env )
    {
        if ( first >= forms.size() ) throw bad();
        value v = nil_t{};
        for ( std::size_t i = first; i < forms.size(); ++ i )
            v = eval( forms[ i ], env );
        return v;
    }

    value apply( const value &f, std::vector< value > &args )
    {
        if ( auto b = std::get_if< const builtin * >( &f ) )
            return ( *b )->fn( args );
        auto l = std::get_if< const lambda * >( &f );
        if ( !l ) throw bad();
        const lambda &c = **l;
        if ( c.params->items.size() != args.size() ) throw bad();
        frame *fr = new_frame( c.env );
        for ( std::size_t i = 0; i < args.size(); ++ i )
            fr->vars[ c.params->items[ i ].atom ] = args[ i ];
        return body( *c.body, c.first, fr );
    }

    value eval( const sexp &e, frame *env )
    {
        if ( e.is_atom() )
        {
            if ( is_number( e.atom ) ) return std::stol( e.atom );
            if ( e.atom == "#t" || e.atom == "#f" ) return e.atom == "#t";
            if ( e.atom == "nil" ) return nil_t{};
            for ( frame *f = env; f; f = f->parent )
                if ( auto it = f->vars.find( e.atom ); it != f->vars.end() )
                    return it->second;
            throw bad();
        }

        const auto &it = e.items;
        if ( it.empty() ) throw bad();
        const std::string &head = it[ 0 ].atom;

        if ( head == "quote" )
        {
            if ( it.size() != 2 ) throw bad();
            return quote( it[ 1 ] );
        }
        if ( head == "lambda" )
        {
            if ( it.size() < 3 || !it[ 1 ].is_list() ) throw bad();
            for ( const auto &p : it[ 1 ].items )
                if ( !p.is_atom() ) throw bad();
            lambdas.push_back( std::make_unique< lambda >( &it[ 1 ], &it, 2, env ) );
            return lambdas.back().get();
        }
        if ( head == "if" )
        {
            if ( it.size() != 4 ) throw bad();
            return truthy( eval( it[ 1 ], env ) ) ? eval( it[ 2 ], env ) : eval( it[ 3 ], env );
        }
        if ( head == "begin" )
            return body( it, 1, env );
        if ( head == "let" )
        {
            if ( it.size() < 3 || !it[ 1 ].is_list() ) throw bad();
            frame *f = new_frame( env );
            for ( const auto &b : it[ 1 ].items )
            {
                if ( !b.is_list() || b.items.size() != 2 || !b.items[ 0 ].is_atom() ) throw bad();
                f->vars[ b.items[ 0 ].atom ] = eval( b.items[ 1 ], env );
            }
            return body( it, 2, f );
        }
        if ( head == "define" )
            throw bad(); /* only at the top level */

        value f = eval( it[ 0 ], env );
        std::vector< value > args;
        for ( std::size_t i = 1; i < it.size(); ++ i )
            args.push_back( eval( it[ i ], env ) );
        return apply( f, args );
    }

    value top( const sexp &form )
    {
        if ( form.is_list() && !form.items.empty() && form.items[ 0 ].atom == "define" )
        {
            if ( form.items.size() != 3 || !form.items[ 1 ].is_atom() ) throw bad();
            global->vars[ form.items[ 1 ].atom ] = eval( form.items[ 2 ], global );
            return symbol{ form.items[ 1 ].atom };
        }
        return eval( form, global );
    }

    static std::string show( const value &v )
    {
        if ( auto i = std::get_if< long >( &v ) ) return std::to_string( *i );
        if ( auto b = std::get_if< bool >( &v ) ) return *b ? "#t" : "#f";
        if ( auto s = std::get_if< symbol >( &v ) ) return s->name;
        if ( std::holds_alternative< nil_t >( v ) ) return "()";
        if ( !std::holds_alternative< const pair * >( v ) ) return "<procedure>";

        std::string out = "(";
        value cur = v;
        while ( auto p = std::get_if< const pair * >( &cur ) )
        {
            out += ( out.size() > 1 ? " " : "" ) + show( ( *p )->car );
            cur = ( *p )->cdr;
        }
        if ( !std::holds_alternative< nil_t >( cur ) )
            out += " . " + show( cur );
        return out + ")";
    }
};

std::optional< std::string > run( std::string_view program )
{
    try
    {
        sexp forms = read( "(" + std::string( program ) + ")" );
        lisp l;
        value v = nil_t{};
        for ( const auto &f : forms.items )
            v = l.top( f );
        return lisp::show( v );
    }
    catch ( const bad & ) { return std::nullopt; }
    catch ( const std::invalid_argument & ) { return std::nullopt; }
}

int main()
{
    assert( run( "42" ) == "42" );
    assert( run( "(define x 5) (* x x)" ) == "25" );
    assert( run( "(define x 5)" ) == "x" );
    assert( run( "(quote (a (b 1) c))" ) == "(a (b 1) c)" );
    assert( run( "(cons 1 (cons 2 3))" ) == "(1 2 . 3)" );
    assert( run( "(list 1 #t (quote x) nil)" ) == "(1 #t x ())" );
    assert( run( "(car (cdr (list 1 2 3)))" ) == "2" );
    assert( run( "+" ) == "<procedure>" );
    assert( run( "(if nil 1 2)" ) == "1" );
    assert( run( "(let ((x 1) (y 2)) (let ((x y) (y x)) (list x y)))" ) == "(2 1)" );
    assert( run( "(eq? (quote a) (quote a))" ) == "#t" );
    assert( run( "(eq? (list 1) (list 1))" ) == "#f" );
    assert( run( "(let ((p (list 1))) (eq? p p))" ) == "#t" );

    auto prelude = "(define map (lambda (f l) (if (null? l) nil (cons (f (car l)) (map f (cdr l))))))"
                   "(define filter (lambda (p l) (if (null? l) nil"
                   "  (if (p (car l)) (cons (car l) (filter p (cdr l))) (filter p (cdr l))))))"
                   "(define foldl (lambda (f a l) (if (null? l) a (foldl f (f a (car l)) (cdr l)))))"
                   "(define range (lambda (a b) (if (< a b) (cons a (range (+ a 1) b)) nil)))"
                   "(define even? (lambda (n) (if (= n 0) #t (odd? (- n 1)))))"
                   "(define odd? (lambda (n) (if (= n 0) #f (even? (- n 1)))))";
    auto with = [ & ]( std::string body ) { return run( prelude + body ); };

    assert( with( "(map (lambda (x) (* x x)) (range 0 5))" ) == "(0 1 4 9 16)" );
    assert( with( "(filter even? (range 0 10))" ) == "(0 2 4 6 8)" );
    assert( with( "(foldl + 0 (range 1 101))" ) == "5050" );
    assert( with( "(foldl (lambda (acc x) (cons x acc)) nil (list 1 2 3))" ) == "(3 2 1)" );
    assert( with( "(map (lambda (f) (f 3)) (list (lambda (x) (+ x 1)) (lambda (x) (* x x))))" ) == "(4 9)" );
    assert( with( "(define adder (lambda (n) (lambda (x) (+ x n)))) (map (adder 10) (list 1 2))" ) == "(11 12)" );
    assert( run( "(define f (lambda () (g))) (define g (lambda () 7)) (f)" ) == "7" );
    assert( run( "(define x 1) (define f (lambda () x)) (define x 2) (f)" ) == "2" );
    assert( run( "(begin 1 2 3)" ) == "3" );

    assert( !run( "y" ) );
    assert( !run( "(car nil)" ) );
    assert( !run( "(+ 1 #t)" ) );
    assert( !run( "((lambda (x) x))" ) );
    assert( !run( "(1 2)" ) );
    assert( !run( "(lambda (x))" ) );
    assert( !run( "(let ((x 1)) (define y 2))" ) );
    assert( !run( "(quote a b)" ) );

    return 0;
}
