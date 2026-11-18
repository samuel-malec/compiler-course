#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>

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

/* In this assignment, you will write the «semantic analysis» for a
 * small typed imperative language with structures and arrays – the
 * phase of a compiler which runs after parsing and finds all the
 * errors that the grammar cannot express. To keep the focus on
 * semantics, programs are written as S-expressions:
 *
 *     program = (program item …) ;
 *     item    = (struct NAME (FIELD type) …)
 *             | (fun NAME ((PARAM type) …) rtype stmt) ;
 *     type    = 'int' | 'bool' | NAME | (array type) ;
 *     rtype   = type | 'void' ;
 *
 *     stmt    = (block stmt …)
 *             | (var x expr) | (var x type expr)
 *             | (set lvalue expr)                 ; lvalue: x, (get …) or (at …)
 *             | (if expr stmt [stmt]) | (while expr stmt)
 *             | (return) | (return expr)
 *             | (do expr) | (print expr) ;
 *
 *     expr    = integer | #t | #f | x
 *             | (op expr expr) | (and expr expr) | (or expr expr) | (not expr)
 *             | (new S expr …)                    ; one value per field, in order
 *             | (get expr FIELD)                  ; field of a struct
 *             | (array type expr …)               ; an array literal
 *             | (at expr expr) | (len expr)       ; indexing, length
 *             | (f expr …) ;                      ; a call
 *
 * The operators are those of Chapter 7: ‹+ - * / %› on ‹int›s, the
 * comparisons ‹< <= > >=› on ‹int›s giving ‹bool›, and ‹=› and ‹!=› on two
 * values of the same type, which must be ‹int› or ‹bool›. Structures and
 * functions can refer to each other in any order; a structure may
 * contain fields of its own type (through arrays, since there is no
 * ‹null›). ‹(var x e)› declares ‹x› with the type of ‹e›; ‹(var x T e)›
 * requires ‹e : T›. Scopes are as in ‹05/p2_resolve›: blocks and the
 * bodies of ‹if› and ‹while› open new scopes, and the parameters are in
 * a scope around the body. A ‹void› function needs no ‹return›, and may
 * only use ‹(return)›; a non-void function must «always return» (as in
 * ‹07/p4_returns›), and only with ‹(return e)›. A call of a ‹void›
 * function may only appear directly in ‹(do …)›. ‹print› takes an ‹int›
 * or a ‹bool›.
 *
 * Write a function ‹check› which returns ‹std::nullopt› for a correct
 * program, and otherwise the «first» error, as ‹LINE:COL: MESSAGE›
 * (positions as in ‹07/p2_errors›). The checks are done in three
 * passes, each over the items in program order:
 *
 *  1. names: ‹duplicate struct S› (at the second ‹(struct›), ‹duplicate
 *     field S.f› (at the second field), ‹duplicate function f›,
 *  2. signatures: ‹unknown type X› (at the atom ‹X›) in field types and in
 *     function signatures (parameters in order, then the result type),
 *     and ‹duplicate variable x› for a repeated parameter (at the
 *     parameter),
 *  3. bodies, left to right, depth first:
 *     ◦ ‹unknown variable x› (at the atom), ‹unknown function f› (at the
 *       call), ‹unknown type X› (in ‹var› and ‹array›), ‹unknown struct S›
 *       (at ‹new›),
 *     ◦ ‹wrong number of arguments› (at a call or ‹new›),
 *     ◦ ‹expected T, got U› – at an expression of type ‹U› where ‹T› is
 *       required: operands, conditions, arguments, field values in
 *       ‹new›, array elements, indices, the value in ‹set› and in an
 *       annotated ‹var›, the value of ‹return› (in a ‹void› function, ‹T›
 *       is ‹void›); a ‹(return)› in a non-‹void› function is reported at
 *       the ‹return› as ‹expected T, got void›; ‹print› of a value of type
 *       ‹U› other than ‹int› and ‹bool› is ‹expected int, got U›,
 *     ◦ ‹cannot compare T› – at the first operand of ‹=› or ‹!=›,
 *     ◦ ‹not a struct: T› (at the operand of ‹get›), ‹no field f in S› (at
 *       the ‹get›), ‹not an array: T› (at the operand of ‹at› or ‹len›),
 *     ◦ ‹void value› – a call of a ‹void› function used as a value (at the
 *       call),
 *     ◦ ‹duplicate variable x› (at the ‹var›), ‹unreachable code› (at the
 *       statement), and finally ‹missing return in f› (at the ‹(fun›),
 *  4. any list which does not have one of the shapes above gives ‹bad
 *     form› (at the list), and an input which cannot be read gives
 *     ‹1:1: syntax error›.
 *
 * Types in messages are written with ‹write› (‹int›, ‹Point›, ‹(array
 * int)›, ‹void›). */

// std::optional< std::string > check( std::string_view program );

struct semantic_error { std::size_t offset; std::string text; };

using type = sexp;
type atom( std::string a ) { return sexp{ a, {}, 0 }; }
std::string show( const type &t ) { return write( t ); }
bool same( const type &a, const type &b ) { return write( a ) == write( b ); }

struct field { std::string name; type t; };
struct structure { std::vector< field > fields; };
struct function { std::vector< type > params; type result; };

[[noreturn]] void error( const sexp &at, std::string text ) { throw semantic_error{ at.offset, std::move( text ) }; }
[[noreturn]] void bad( const sexp &at ) { error( at, "bad form" ); }

struct analysis
{
    std::map< std::string, structure > structs;
    std::map< std::string, function > funs;

    /* the function being checked */
    std::string fname;
    type result = atom( "void" );
    std::vector< std::map< std::string, type > > scopes = {};

    void valid( const type &t, bool allow_void = false )
    {
        if ( t.is_atom() )
        {
            if ( t.atom == "int" || t.atom == "bool" || structs.contains( t.atom ) ||
                 ( allow_void && t.atom == "void" ) )
                return;
            error( t, "unknown type " + t.atom );
        }
        if ( t.items.size() != 2 || t.items[ 0 ].atom != "array" ) bad( t );
        valid( t.items[ 1 ] );
    }

    type lookup( const sexp &x )
    {
        for ( auto it = scopes.rbegin(); it != scopes.rend(); ++ it )
            if ( auto v = it->find( x.atom ); v != it->end() ) return v->second;
        error( x, "unknown variable " + x.atom );
    }

    void declare( const sexp &at, const std::string &name, const type &t )
    {
        if ( !scopes.back().emplace( name, t ).second )
            error( at, "duplicate variable " + name );
    }

    /* expressions; ‹void› results are only allowed if ‹allow_void› */

    void expect( const sexp &e, const type &t )
    {
        type u = expr( e );
        if ( !same( t, u ) ) error( e, "expected " + show( t ) + ", got " + show( u ) );
    }

    type expr( const sexp &e, bool allow_void = false )
    {
        const type int_t = atom( "int" ), bool_t = atom( "bool" );

        if ( e.is_atom() )
        {
            if ( e.atom == "#t" || e.atom == "#f" ) return bool_t;
            if ( e.atom.find_first_not_of( "-0123456789" ) == e.atom.npos && e.atom != "-" ) return int_t;
            return lookup( e );
        }

        const auto &it = e.items;
        if ( it.empty() || !it[ 0 ].is_atom() ) bad( e );
        const std::string &h = it[ 0 ].atom;
        auto arity = [ & ]( std::size_t n ) { if ( it.size() != n + 1 ) bad( e ); };

        if ( h == "+" || h == "-" || h == "*" || h == "/" || h == "%" ||
             h == "<" || h == "<=" || h == ">" || h == ">=" )
        {
            arity( 2 );
            expect( it[ 1 ], int_t );
            expect( it[ 2 ], int_t );
            return h.size() == 1 && h != "<" && h != ">" ? int_t : bool_t;
        }
        if ( h == "=" || h == "!=" )
        {
            arity( 2 );
            type t = expr( it[ 1 ] );
            if ( !same( t, int_t ) && !same( t, bool_t ) ) error( it[ 1 ], "cannot compare " + show( t ) );
            expect( it[ 2 ], t );
            return bool_t;
        }
        if ( h == "and" || h == "or" || h == "not" )
        {
            arity( h == "not" ? 1 : 2 );
            for ( std::size_t i = 1; i < it.size(); ++ i ) expect( it[ i ], bool_t );
            return bool_t;
        }
        if ( h == "new" )
        {
            if ( it.size() < 2 || !it[ 1 ].is_atom() ) bad( e );
            auto s = structs.find( it[ 1 ].atom );
            if ( s == structs.end() ) error( e, "unknown struct " + it[ 1 ].atom );
            if ( s->second.fields.size() != it.size() - 2 ) error( e, "wrong number of arguments" );
            for ( std::size_t i = 2; i < it.size(); ++ i ) expect( it[ i ], s->second.fields[ i - 2 ].t );
            return it[ 1 ];
        }
        if ( h == "get" )
        {
            arity( 2 );
            if ( !it[ 2 ].is_atom() ) bad( e );
            type t = expr( it[ 1 ] );
            if ( !t.is_atom() || !structs.contains( t.atom ) ) error( it[ 1 ], "not a struct: " + show( t ) );
            for ( const auto &f : structs.at( t.atom ).fields )
                if ( f.name == it[ 2 ].atom ) return f.t;
            error( e, "no field " + it[ 2 ].atom + " in " + t.atom );
        }
        if ( h == "array" )
        {
            if ( it.size() < 2 ) bad( e );
            valid( it[ 1 ] );
            for ( std::size_t i = 2; i < it.size(); ++ i ) expect( it[ i ], it[ 1 ] );
            return sexp{ "", { atom( "array" ), it[ 1 ] }, 0 };
        }
        if ( h == "at" || h == "len" )
        {
            arity( h == "at" ? 2 : 1 );
            type a = expr( it[ 1 ] );
            if ( !a.is_list() ) error( it[ 1 ], "not an array: " + show( a ) );
            if ( h == "len" ) return int_t;
            expect( it[ 2 ], int_t );
            return a.items[ 1 ];
        }

        auto f = funs.find( h );
        if ( f == funs.end() ) error( e, "unknown function " + h );
        if ( f->second.params.size() != it.size() - 1 ) error( e, "wrong number of arguments" );
        for ( std::size_t i = 1; i < it.size(); ++ i ) expect( it[ i ], f->second.params[ i - 1 ] );
        if ( !allow_void && same( f->second.result, atom( "void" ) ) ) error( e, "void value" );
        return f->second.result;
    }

    /* statements; returns whether the statement always returns */

    bool scoped( const sexp &s )
    {
        scopes.emplace_back();
        bool r = stmt( s );
        scopes.pop_back();
        return r;
    }

    bool stmt( const sexp &s )
    {
        if ( s.is_atom() || s.items.empty() || !s.items[ 0 ].is_atom() ) bad( s );
        const auto &it = s.items;
        const std::string &h = it[ 0 ].atom;
        const type int_t = atom( "int" ), bool_t = atom( "bool" );

        if ( h == "block" )
        {
            scopes.emplace_back();
            bool returns = false;
            for ( std::size_t i = 1; i < it.size(); ++ i )
            {
                if ( returns ) error( it[ i ], "unreachable code" );
                returns = stmt( it[ i ] );
            }
            scopes.pop_back();
            return returns;
        }
        if ( h == "var" )
        {
            if ( ( it.size() != 3 && it.size() != 4 ) || !it[ 1 ].is_atom() ) bad( s );
            type t;
            if ( it.size() == 4 )
            {
                valid( it[ 2 ] );
                expect( it[ 3 ], it[ 2 ] );
                t = it[ 2 ];
            }
            else
                t = expr( it[ 2 ] );
            declare( s, it[ 1 ].atom, t );
            return false;
        }
        if ( h == "set" )
        {
            if ( it.size() != 3 ) bad( s );
            const sexp &lv = it[ 1 ];
            bool lvalue = lv.is_atom() ? lv.atom.find_first_not_of( "-0123456789" ) != lv.atom.npos &&
                                         lv.atom != "#t" && lv.atom != "#f"
                                       : !lv.items.empty() && ( lv.items[ 0 ].atom == "get" || lv.items[ 0 ].atom == "at" );
            if ( !lvalue ) bad( s );
            expect( it[ 2 ], expr( lv ) );
            return false;
        }
        if ( h == "if" )
        {
            if ( it.size() != 3 && it.size() != 4 ) bad( s );
            expect( it[ 1 ], bool_t );
            bool a = scoped( it[ 2 ] );
            bool b = it.size() == 4 && scoped( it[ 3 ] );
            return a && b;
        }
        if ( h == "while" )
        {
            if ( it.size() != 3 ) bad( s );
            expect( it[ 1 ], bool_t );
            scoped( it[ 2 ] );
            return false;
        }
        if ( h == "return" )
        {
            if ( it.size() > 2 ) bad( s );
            if ( it.size() == 1 )
            {
                if ( !same( result, atom( "void" ) ) )
                    error( s, "expected " + show( result ) + ", got void" );
            }
            else
            {
                type t = expr( it[ 1 ] );
                if ( !same( t, result ) ) error( it[ 1 ], "expected " + show( result ) + ", got " + show( t ) );
            }
            return true;
        }
        if ( h == "do" )
        {
            if ( it.size() != 2 ) bad( s );
            expr( it[ 1 ], true );
            return false;
        }
        if ( h == "print" )
        {
            if ( it.size() != 2 ) bad( s );
            type t = expr( it[ 1 ] );
            if ( !same( t, int_t ) && !same( t, bool_t ) ) error( it[ 1 ], "expected int, got " + show( t ) );
            return false;
        }
        bad( s );
    }

    void run( const sexp &p )
    {
        if ( !p.is_list() || p.items.empty() || p.items[ 0 ].atom != "program" ) bad( p );

        /* pass 1: names */
        std::set< std::string > fun_names;
        for ( std::size_t i = 1; i < p.items.size(); ++ i )
        {
            const sexp &item = p.items[ i ];
            if ( item.is_atom() || item.items.size() < 2 || !item.items[ 1 ].is_atom() ) bad( item );
            const std::string &kind = item.items[ 0 ].atom, &name = item.items[ 1 ].atom;

            if ( kind == "struct" )
            {
                if ( structs.contains( name ) ) error( item, "duplicate struct " + name );
                structure s;
                for ( std::size_t j = 2; j < item.items.size(); ++ j )
                {
                    const sexp &f = item.items[ j ];
                    if ( !f.is_list() || f.items.size() != 2 || !f.items[ 0 ].is_atom() ) bad( f );
                    for ( const auto &g : s.fields )
                        if ( g.name == f.items[ 0 ].atom ) error( f, "duplicate field " + name + "." + g.name );
                    s.fields.push_back( { f.items[ 0 ].atom, f.items[ 1 ] } );
                }
                structs.emplace( name, s );
            }
            else if ( kind == "fun" )
            {
                if ( item.items.size() != 5 || !item.items[ 2 ].is_list() ) bad( item );
                if ( !fun_names.insert( name ).second ) error( item, "duplicate function " + name );
            }
            else
                bad( item );
        }

        /* pass 2: signatures */
        for ( std::size_t i = 1; i < p.items.size(); ++ i )
        {
            const sexp &item = p.items[ i ];
            if ( item.items[ 0 ].atom == "struct" )
            {
                for ( std::size_t j = 2; j < item.items.size(); ++ j )
                    valid( item.items[ j ].items[ 1 ] );
                continue;
            }

            function f;
            std::set< std::string > seen;
            for ( const auto &param : item.items[ 2 ].items )
            {
                if ( !param.is_list() || param.items.size() != 2 || !param.items[ 0 ].is_atom() ) bad( param );
                valid( param.items[ 1 ] );
                if ( !seen.insert( param.items[ 0 ].atom ).second )
                    error( param, "duplicate variable " + param.items[ 0 ].atom );
                f.params.push_back( param.items[ 1 ] );
            }
            valid( item.items[ 3 ], true );
            f.result = item.items[ 3 ];
            funs.emplace( item.items[ 1 ].atom, f );
        }

        /* pass 3: bodies */
        for ( std::size_t i = 1; i < p.items.size(); ++ i )
        {
            const sexp &item = p.items[ i ];
            if ( item.items[ 0 ].atom != "fun" ) continue;

            fname = item.items[ 1 ].atom;
            result = item.items[ 3 ];
            scopes = { {} };
            for ( const auto &param : item.items[ 2 ].items )
                scopes.back().emplace( param.items[ 0 ].atom, param.items[ 1 ] );

            bool returns = scoped( item.items[ 4 ] );
            if ( !returns && !same( result, atom( "void" ) ) )
                error( item, "missing return in " + fname );
        }
    }
};

std::optional< std::string > check( std::string_view program )
{
    sexp p;
    try { p = read( program ); }
    catch ( const std::invalid_argument & ) { return "1:1: syntax error"; }

    try
    {
        analysis a;
        a.run( p );
        return std::nullopt;
    }
    catch ( const semantic_error &e )
    {
        int line = 1, col = 1;
        for ( std::size_t i = 0; i < e.offset; ++ i )
            program[ i ] == '\n' ? ( ++ line, col = 1 ) : ++ col;
        return std::to_string( line ) + ":" + std::to_string( col ) + ": " + e.text;
    }
}

int main()
{
    auto ok = []( std::string_view p ) { return !check( p ); };
    auto wrap = []( std::string body ) { return "(program " + body + ")"; };

    std::string point = "(struct Point (x int) (y int))";
    std::string shapes =
        "(program"
        " (struct Tree (value int) (kids (array Tree)))"
        " (fun sum ((t Tree)) int (block (var s (get t value)) (var i 0)"
        "   (while (< i (len (get t kids))) (block (set s (+ s (sum (at (get t kids) i)))) (set i (+ i 1))))"
        "   (return s)))"
        " (fun leaf ((v int)) Tree (return (new Tree v (array Tree))))"
        " (fun main () void (block (var t (new Tree 1 (array Tree (leaf 2) (leaf 3))))"
        "   (set (at (get t kids) 0) (leaf 10)) (print (sum t)) (do (show t)) (return)))"
        " (fun show ((t Tree)) void (print (= (sum t) 0))))";
    assert( ok( shapes ) );

    assert( ok( wrap( point + " (fun f ((p Point)) bool (if (< (get p x) 0) (return #t) (return (= (get p y) 0))))" ) ) );
    assert( ok( wrap( "(fun f () void (block))" ) ) );

    assert( check( wrap( point + "\n" + point ) ) == "2:1: duplicate struct Point" );
    assert( check( wrap( "(struct S (a int) (a bool))" ) ) == "1:28: duplicate field S.a" );
    assert( check( wrap( "(fun f () void (do 1)) (fun f () void (do 2))" ) ) == "1:33: duplicate function f" );
    assert( check( wrap( "(struct S (a Q))" ) ) == "1:23: unknown type Q" );
    assert( check( wrap( "(fun f ((a int) (a int)) void (return))" ) ) == "1:26: duplicate variable a" );
    assert( check( wrap( "(fun f () int (return y))" ) ) == "1:32: unknown variable y" );
    assert( check( wrap( "(fun f () int (return (g)))" ) ) == "1:32: unknown function g" );
    assert( check( wrap( "(fun f () int (return (+ 1 #t)))" ) ) == "1:37: expected int, got bool" );
    assert( check( wrap( "(fun f () void (return 1))" ) ) == "1:33: expected void, got int" );
    assert( check( wrap( "(fun f () int (return))" ) ) == "1:24: expected int, got void" );
    assert( check( wrap( "(fun f () int (if #t (return 1)))" ) ) == "1:10: missing return in f" );
    assert( check( wrap( "(fun f () int (block (return 1) (do 2)))" ) ) == "1:42: unreachable code" );
    assert( check( wrap( point + " (fun f ((p Point)) int (return (get p z)))" ) ) == "1:72: no field z in Point" );
    assert( check( wrap( "(fun f ((a int)) int (return (get a x)))" ) ) == "1:44: not a struct: int" );
    assert( check( wrap( "(fun f ((a int)) int (return (len a)))" ) ) == "1:44: not an array: int" );
    assert( check( wrap( point + " (fun f ((p Point)) bool (return (= p p)))" ) ) == "1:76: cannot compare Point" );
    assert( check( wrap( "(fun g () void (return)) (fun f () int (return (g)))" ) ) == "1:57: void value" );
    assert( check( wrap( "(fun f () void (block (var a 1) (var a 2)))" ) ) == "1:42: duplicate variable a" );
    assert( check( wrap( point + " (fun f () Point (return (new Point 1)))" ) ) == "1:65: wrong number of arguments" );
    assert( check( wrap( "(fun f () void (print (array int)))" ) ) == "1:32: expected int, got (array int)" );
    assert( check( wrap( "(fun f () void (do (array int 1 #f)))" ) ) == "1:42: expected int, got bool" );
    assert( check( wrap( "(fun f () void (set 1 2))" ) ) == "1:25: bad form" );
    assert( check( "(program" ) == "1:1: syntax error" );

    return 0;
}
