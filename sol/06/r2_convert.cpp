#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
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

/* Machine code has no closures: a function is just an address of
 * some code. A compiler for a language with closures therefore
 * performs «closure conversion»: each function becomes a piece of
 * top-level code which receives its free variables explicitly, in an
 * «environment» record, and a closure becomes a pair of a code pointer
 * and such a record. Write a function ‹convert› which does this for
 * the language of ‹e1_apply›:
 *
 *  • each ‹(fn (p …) b)› in the program gets a label ‹Lk›, numbered
 *    from 0 in «pre-order» (in the order in which the ‹(fn› appears in
 *    the program text),
 *  • the function expression is replaced by ‹(closure Lk v₁ … vₘ)›,
 *    where ‹v₁ … vₘ› are the «free variables» of the function (see
 *    ‹e3_free›), sorted alphabetically, each of them converted as a
 *    variable reference «in the context of the ‹closure› expression»,
 *  • inside the body of the function, a reference to the ‹i›-th free
 *    variable becomes ‹(env i)› (counting from 0); references to the
 *    parameters, and to variables bound by ‹let›s inside the body, stay
 *    as they are,
 *  • every call ‹(f a …)› becomes ‹(call f a …)›,
 *  • numbers, operators, ‹if› and ‹let› are unchanged (except that their
 *    sub-expressions are converted).
 *
 * The result is a list of strings: for each function, in label order,
 * ‹Lk = (code (p …) B)› where ‹B› is the converted body; and finally
 * ‹main = E› where ‹E› is the converted program. Everything is written
 * with ‹write›. You can assume that the input is well formed. */

// std::vector< std::string > convert( std::string_view program );

bool is_number( const std::string &a ) { return a.find_first_not_of( "-0123456789" ) == a.npos; }

bool is_form( const std::string &h )
{
    static const std::string_view forms[] =
        { "+", "-", "*", "/", "%", "<", "<=", ">", ">=", "=", "!=", "if" };
    for ( auto f : forms ) if ( h == f ) return true;
    return false;
}

std::set< std::string > free_vars( const sexp &e )
{
    if ( e.is_atom() )
        return is_number( e.atom ) ? std::set< std::string >{} : std::set< std::string >{ e.atom };
    const auto &it = e.items;
    const std::string &head = it[ 0 ].atom;
    std::set< std::string > out;
    if ( head == "fn" )
    {
        out = free_vars( it[ 2 ] );
        for ( const auto &p : it[ 1 ].items ) out.erase( p.atom );
        return out;
    }
    if ( head == "let" )
    {
        out = free_vars( it[ 3 ] );
        out.erase( it[ 1 ].atom );
        out.merge( free_vars( it[ 2 ] ) );
        return out;
    }
    for ( std::size_t i = it[ 0 ].is_atom() && is_form( head ) ? 1 : 0; i < it.size(); ++ i )
        out.merge( free_vars( it[ i ] ) );
    return out;
}

sexp atom( std::string a ) { return sexp{ a, {}, 0 }; }

struct converter
{
    std::vector< std::string > code;
    int next = 0;

    /* ‹env› lists the free variables of the function we are inside (in
     * order); ‹local› the names bound inside it (params and lets) which
     * currently shadow them. A name refers to the environment if it is
     * in ‹env› and not shadowed. */

    sexp convert( const sexp &e, const std::vector< std::string > &env,
                  std::set< std::string > local )
    {
        if ( e.is_atom() )
        {
            if ( is_number( e.atom ) || local.contains( e.atom ) )
                return e;
            for ( std::size_t i = 0; i < env.size(); ++ i )
                if ( env[ i ] == e.atom )
                    return sexp{ "", { atom( "env" ), atom( std::to_string( i ) ) }, 0 };
            return e;
        }

        const auto &it = e.items;
        const std::string &head = it[ 0 ].atom;

        if ( head == "fn" )
        {
            int label = next++;
            std::size_t slot = code.size();
            code.emplace_back(); /* reserve, so that labels stay in pre-order */

            auto fv = free_vars( e );
            std::vector< std::string > inner_env( fv.begin(), fv.end() );
            std::set< std::string > inner_local;
            for ( const auto &p : it[ 1 ].items ) inner_local.insert( p.atom );

            sexp body = convert( it[ 2 ], inner_env, inner_local );
            code[ slot ] = "L" + std::to_string( label ) + " = (code " + write( it[ 1 ] ) +
                           " " + write( body ) + ")";

            sexp clo{ "", { atom( "closure" ), atom( "L" + std::to_string( label ) ) }, 0 };
            for ( const auto &v : inner_env )
                clo.items.push_back( convert( atom( v ), env, local ) );
            return clo;
        }

        if ( head == "let" )
        {
            sexp value = convert( it[ 2 ], env, local );
            local.insert( it[ 1 ].atom );
            return sexp{ "", { it[ 0 ], it[ 1 ], value, convert( it[ 3 ], env, local ) }, 0 };
        }

        sexp out{ "", {}, 0 };
        if ( it[ 0 ].is_atom() && is_form( head ) )
            out.items.push_back( it[ 0 ] );
        else
            out.items.push_back( atom( "call" ) ), out.items.push_back( convert( it[ 0 ], env, local ) );
        for ( std::size_t i = 1; i < it.size(); ++ i )
            out.items.push_back( convert( it[ i ], env, local ) );
        return out;
    }
};

std::vector< std::string > convert( std::string_view program )
{
    converter c;
    sexp main = c.convert( read( program ), {}, {} );
    c.code.push_back( "main = " + write( main ) );
    return c.code;
}

int main()
{
    using v = std::vector< std::string >;

    assert(( convert( "(+ 1 2)" ) == v{ "main = (+ 1 2)" } ));
    assert(( convert( "((fn (x) x) 5)" ) ==
             v{ "L0 = (code (x) x)", "main = (call (closure L0) 5)" } ));

    assert(( convert( "(let add (fn (x) (fn (y) (+ x y))) ((add 1) 2))" ) ==
             v{ "L0 = (code (x) (closure L1 x))",
                "L1 = (code (y) (+ (env 0) y))",
                "main = (let add (closure L0) (call (call add 1) 2))" } ));

    assert(( convert( "(fn (a) (fn (b) (fn (c) (+ a (+ b c)))))" ) ==
             v{ "L0 = (code (a) (closure L1 a))",
                "L1 = (code (b) (closure L2 (env 0) b))",
                "L2 = (code (c) (+ (env 0) (+ (env 1) c)))",
                "main = (closure L0)" } ));

    assert(( convert( "(let k 10 (let f (fn (x) (let y (* x k) (fn () (+ y z)))) (f 1)))" ) ==
             v{ "L0 = (code (x) (let y (* x (env 0)) (closure L1 y (env 1))))",
                "L1 = (code () (+ (env 0) (env 1)))",
                "main = (let k 10 (let f (closure L0 k z) (call f 1)))" } ));

    assert(( convert( "(fn (f) (if f (f 1) ((fn (g) (g f)) f)))" ) ==
             v{ "L0 = (code (f) (if f (call f 1) (call (closure L1 f) f)))",
                "L1 = (code (g) (call g (env 0)))",
                "main = (closure L0)" } ));

    return 0;
}
