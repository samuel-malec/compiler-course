#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <algorithm>
#include <memory>
#include <set>

/* In this assignment, you will implement a small query language in
 * the style of SQL, over tables stored in memory. A table is a list
 * of rows, and each row maps column names to values, which are
 * either integers or strings. All rows of a table have the same set
 * of columns. */

using value = std::variant< long, std::string >;
using row = std::map< std::string, value >;
using table = std::vector< row >;
using database = std::map< std::string, table >;

/* The result of a query is a list of column names and a list of
 * result rows, each with one value per column. */

struct result
{
    std::vector< std::string > columns;
    std::vector< std::vector< value > > rows;
};

/* The syntax of queries:
 *
 *     query = 'select', items, 'from', ident, [ 'where', expr ],
 *             [ 'order', 'by', expr, [ 'asc' | 'desc' ] ],
 *             [ 'limit', integer ] ;
 *     items = '*' | item, { ',', item } ;
 *     item  = expr, [ 'as', ident ] ;
 *
 *     expr  = conj, { 'or', conj } ;
 *     conj  = neg, { 'and', neg } ;
 *     neg   = 'not', neg | cmp ;
 *     cmp   = sum, [ ( '=' | '!=' | '<' | '<=' | '>' | '>=' ), sum ] ;
 *     sum   = prod, { ( '+' | '-' ), prod } ;
 *     prod  = unary, { ( '*' | '/' | '%' ), unary } ;
 *     unary = '-', unary | atom ;
 *     atom  = integer | string | ident, [ '(', expr, ')' ]
 *           | '(', expr, ')' ;
 *
 * Lexical rules: the «keywords» ‹select›, ‹from›, ‹where›, ‹order›,
 * ‹by›, ‹asc›, ‹desc›, ‹limit›, ‹as›, ‹and›, ‹or› and ‹not› are
 * case-insensitive (‹SELECT› and ‹Select› are fine too); an
 * «identifier» is a letter or ‹_› followed by letters, digits and ‹_›
 * which is not a keyword (identifiers «are» case-sensitive); an
 * «integer» is a sequence of digits; a «string» is enclosed in single
 * quotes, and a quote inside it is written as two quotes (‹'it''s'›).
 * Whitespace separates tokens. ‹ident( expr )› is a function call;
 * the functions are ‹length› (of a string, giving an integer), and
 * ‹upper› and ‹lower› (of a string, giving a string; only ASCII
 * letters are converted).
 *
 * The semantics:
 *
 *  • An identifier (other than a function name) refers to the column
 *    of that name in the current row.
 *  • ‹+› adds two integers or concatenates two strings; ‹-›, ‹*›, ‹/›
 *    and ‹%› work on integers only (‹/› and ‹%› as in C++); unary ‹-›
 *    negates an integer.
 *  • Comparisons work on two integers or two strings (compared as by
 *    ‹<› on ‹std::string›) and give 1 (true) or 0 (false).
 *  • ‹and›, ‹or› and ‹not› work on integers (non-zero is true) and
 *    give 1 or 0; ‹and› and ‹or› do not evaluate their right operand
 *    if the left one decides the result.
 *  • ‹where› keeps the rows for which the condition is a non-zero
 *    integer; ‹order by› sorts the remaining rows by the value of the
 *    key expression, in ascending (the default) or descending order;
 *    the sort must be «stable» (rows with equal keys stay in the
 *    order of the table) in both directions; ‹limit n› keeps the first
 *    ‹n› rows (after sorting).
 *  • ‹*› selects all columns of the table, in alphabetical order (the
 *    order of ‹std::map›; if the table is empty, there are no
 *    columns). Otherwise, the name of each result column is its alias
 *    (given by ‹as›), or the name of the column if the expression is
 *    just an identifier, or ‹colN› where ‹N› is the position of the
 *    item, counted from 1.
 *
 * Return ‹std::nullopt› for a syntax error, an unknown table, and for
 * any error during evaluation: an unknown column or function, a type
 * error (including a ‹where› condition or an ‹order by› key of the
 * wrong type, and keys of different types in one query), or division
 * by zero. Only expressions which are actually evaluated can fail –
 * in particular, nothing is evaluated for an empty table. */

// std::optional< result > query( const database &db, std::string_view q );

struct query_error {};

/* ─── lexer ──────────────────────────────────────────────────────── */

enum class qtok { keyword, ident, integer, string, symbol, end };

struct qtoken
{
    qtok kind;
    std::string text; /* keywords are lowercased */
};

std::vector< qtoken > qlex( std::string_view s )
{
    static const std::set< std::string > keywords =
    {
        "select", "from", "where", "order", "by", "asc", "desc",
        "limit", "as", "and", "or", "not"
    };

    auto alpha = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_'; };
    auto digit = []( char c ) { return c >= '0' && c <= '9'; };

    std::vector< qtoken > out;
    std::size_t i = 0;

    while ( i < s.size() )
    {
        std::size_t start = i;
        char c = s[ i++ ];

        if ( c == ' ' || c == '\n' || c == '\t' )
            continue;

        if ( alpha( c ) )
        {
            while ( i < s.size() && ( alpha( s[ i ] ) || digit( s[ i ] ) ) ) ++ i;
            std::string word( s.substr( start, i - start ) ), lower = word;
            for ( char &x : lower )
                if ( x >= 'A' && x <= 'Z' ) x += 'a' - 'A';
            if ( keywords.contains( lower ) )
                out.push_back( { qtok::keyword, lower } );
            else
                out.push_back( { qtok::ident, word } );
        }
        else if ( digit( c ) )
        {
            while ( i < s.size() && digit( s[ i ] ) ) ++ i;
            out.push_back( { qtok::integer, std::string( s.substr( start, i - start ) ) } );
        }
        else if ( c == '\'' )
        {
            std::string text;
            while ( true )
            {
                if ( i >= s.size() )
                    throw query_error();
                if ( s[ i ] == '\'' )
                {
                    if ( i + 1 < s.size() && s[ i + 1 ] == '\'' )
                        text += '\'', i += 2;
                    else
                        break;
                }
                else
                    text += s[ i++ ];
            }
            ++ i;
            out.push_back( { qtok::string, text } );
        }
        else if ( ( c == '<' || c == '>' || c == '!' ) && i < s.size() && s[ i ] == '=' )
            out.push_back( { qtok::symbol, std::string( s.substr( start, ++ i - start ) ) } );
        else if ( std::string_view( "*,()=<>+-/%" ).find( c ) != s.npos )
            out.push_back( { qtok::symbol, std::string( 1, c ) } );
        else
            throw query_error();
    }

    out.push_back( { qtok::end, "" } );
    return out;
}

/* ─── syntax ─────────────────────────────────────────────────────── */

struct qnode;
using qnode_ptr = std::shared_ptr< const qnode >;

struct qnode
{
    enum kind { literal, column, call, unary, binary } k;
    value lit = 0L;
    std::string name = {};               /* column, function or operator */
    qnode_ptr a = nullptr, b = nullptr;
};

struct item
{
    qnode_ptr expr;
    std::string name;
};

struct parsed_query
{
    bool star = false;
    std::vector< item > items = {};
    std::string table = {};
    qnode_ptr where = nullptr, order = nullptr;
    bool descending = false;
    std::optional< long > limit = std::nullopt;
};

struct qparser
{
    std::vector< qtoken > toks;
    std::size_t pos = 0;

    const qtoken &peek() const { return toks[ pos ]; }
    qtoken next() { return toks[ pos < toks.size() - 1 ? pos++ : pos ]; }

    bool accept_kw( std::string_view kw )
    {
        if ( peek().kind != qtok::keyword || peek().text != kw ) return false;
        return next(), true;
    }

    bool accept_sym( std::string_view sym )
    {
        if ( peek().kind != qtok::symbol || peek().text != sym ) return false;
        return next(), true;
    }

    void expect_kw( std::string_view kw ) { if ( !accept_kw( kw ) ) throw query_error(); }
    void expect_sym( std::string_view s ) { if ( !accept_sym( s ) ) throw query_error(); }

    std::string ident()
    {
        if ( peek().kind != qtok::ident ) throw query_error();
        return next().text;
    }

    static qnode_ptr bin( std::string op, qnode_ptr a, qnode_ptr b )
    {
        return std::make_shared< qnode >( qnode::binary, 0L, op, a, b );
    }

    qnode_ptr expr()
    {
        auto l = conj();
        while ( accept_kw( "or" ) ) l = bin( "or", l, conj() );
        return l;
    }

    qnode_ptr conj()
    {
        auto l = neg();
        while ( accept_kw( "and" ) ) l = bin( "and", l, neg() );
        return l;
    }

    qnode_ptr neg()
    {
        if ( accept_kw( "not" ) )
            return std::make_shared< qnode >( qnode::unary, 0L, "not", neg() );
        return cmp();
    }

    qnode_ptr cmp()
    {
        auto l = sum();
        for ( auto op : { "=", "!=", "<", "<=", ">", ">=" } )
            if ( accept_sym( op ) )
                return bin( op, l, sum() );
        return l;
    }

    qnode_ptr sum()
    {
        auto l = prod();
        while ( true )
            if ( accept_sym( "+" ) ) l = bin( "+", l, prod() );
            else if ( accept_sym( "-" ) ) l = bin( "-", l, prod() );
            else return l;
    }

    qnode_ptr prod()
    {
        auto l = unary();
        while ( true )
            if ( accept_sym( "*" ) ) l = bin( "*", l, unary() );
            else if ( accept_sym( "/" ) ) l = bin( "/", l, unary() );
            else if ( accept_sym( "%" ) ) l = bin( "%", l, unary() );
            else return l;
    }

    qnode_ptr unary()
    {
        if ( accept_sym( "-" ) )
            return std::make_shared< qnode >( qnode::unary, 0L, "-", unary() );
        return atom();
    }

    qnode_ptr atom()
    {
        if ( accept_sym( "(" ) )
        {
            auto e = expr();
            expect_sym( ")" );
            return e;
        }

        qtoken t = next();
        switch ( t.kind )
        {
            case qtok::integer: return std::make_shared< qnode >( qnode::literal, std::stol( t.text ) );
            case qtok::string:  return std::make_shared< qnode >( qnode::literal, t.text );
            case qtok::ident:
                if ( accept_sym( "(" ) )
                {
                    auto arg = expr();
                    expect_sym( ")" );
                    return std::make_shared< qnode >( qnode::call, 0L, t.text, arg );
                }
                return std::make_shared< qnode >( qnode::column, 0L, t.text );
            default:
                throw query_error();
        }
    }

    parsed_query query()
    {
        parsed_query q;
        expect_kw( "select" );

        if ( accept_sym( "*" ) )
            q.star = true;
        else
            do
            {
                auto e = expr();
                std::string name = "col" + std::to_string( q.items.size() + 1 );
                if ( e->k == qnode::column )
                    name = e->name;
                if ( accept_kw( "as" ) )
                    name = ident();
                q.items.push_back( { e, name } );
            } while ( accept_sym( "," ) );

        expect_kw( "from" );
        q.table = ident();

        if ( accept_kw( "where" ) )
            q.where = expr();

        if ( accept_kw( "order" ) )
        {
            expect_kw( "by" );
            q.order = expr();
            if ( accept_kw( "desc" ) )
                q.descending = true;
            else
                accept_kw( "asc" );
        }

        if ( accept_kw( "limit" ) )
        {
            if ( peek().kind != qtok::integer )
                throw query_error();
            q.limit = std::stol( next().text );
        }

        if ( peek().kind != qtok::end )
            throw query_error();
        return q;
    }
};

/* ─── evaluation ─────────────────────────────────────────────────── */

long as_int( const value &v )
{
    if ( auto i = std::get_if< long >( &v ) ) return *i;
    throw query_error();
}

const std::string &as_str( const value &v )
{
    if ( auto s = std::get_if< std::string >( &v ) ) return *s;
    throw query_error();
}

value eval( const qnode &n, const row &r )
{
    switch ( n.k )
    {
        case qnode::literal: return n.lit;

        case qnode::column:
        {
            auto it = r.find( n.name );
            if ( it == r.end() ) throw query_error();
            return it->second;
        }

        case qnode::call:
        {
            std::string s = as_str( eval( *n.a, r ) );
            if ( n.name == "length" ) return long( s.size() );
            if ( n.name != "upper" && n.name != "lower" ) throw query_error();
            for ( char &c : s )
                if ( n.name == "upper" && c >= 'a' && c <= 'z' ) c -= 'a' - 'A';
                else if ( n.name == "lower" && c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
            return s;
        }

        case qnode::unary:
        {
            long x = as_int( eval( *n.a, r ) );
            return n.name == "-" ? -x : long( x == 0 );
        }

        case qnode::binary: break;
    }

    const std::string &op = n.name;

    if ( op == "and" || op == "or" )
    {
        bool l = as_int( eval( *n.a, r ) ) != 0;
        if ( l == ( op == "or" ) )
            return long( l );
        return long( as_int( eval( *n.b, r ) ) != 0 );
    }

    value a = eval( *n.a, r ), b = eval( *n.b, r );

    if ( op == "+" && std::holds_alternative< std::string >( a ) )
        return as_str( a ) + as_str( b );

    if ( op == "=" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=" )
    {
        if ( a.index() != b.index() ) throw query_error();
        bool res = op == "=" ? a == b : op == "!=" ? a != b : op == "<" ? a < b :
                   op == "<=" ? a <= b : op == ">" ? a > b : a >= b;
        return long( res );
    }

    long x = as_int( a ), y = as_int( b );
    if ( op == "+" ) return x + y;
    if ( op == "-" ) return x - y;
    if ( op == "*" ) return x * y;
    if ( y == 0 ) throw query_error();
    return op == "/" ? x / y : x % y;
}

std::optional< result > query( const database &db, std::string_view text )
{
    try
    {
        qparser p{ qlex( text ) };
        parsed_query q = p.query();

        auto t = db.find( q.table );
        if ( t == db.end() )
            return std::nullopt;

        std::vector< const row * > rows;
        for ( const auto &r : t->second )
            if ( !q.where || as_int( eval( *q.where, r ) ) != 0 )
                rows.push_back( &r );

        if ( q.order )
        {
            std::vector< std::pair< value, const row * > > keyed;
            for ( auto r : rows )
            {
                keyed.emplace_back( eval( *q.order, *r ), r );
                if ( keyed.front().first.index() != keyed.back().first.index() )
                    throw query_error();
            }

            std::stable_sort( keyed.begin(), keyed.end(), [ & ]( auto &x, auto &y )
            {
                return q.descending ? y.first < x.first : x.first < y.first;
            } );

            rows.clear();
            for ( auto &[ _, r ] : keyed )
                rows.push_back( r );
        }

        if ( q.limit && rows.size() > std::size_t( *q.limit ) )
            rows.resize( *q.limit );

        result res;

        if ( q.star )
        {
            if ( !t->second.empty() )
                for ( const auto &[ col, _ ] : t->second.front() )
                    res.columns.push_back( col );
            for ( auto r : rows )
            {
                res.rows.emplace_back();
                for ( const auto &[ _, v ] : *r )
                    res.rows.back().push_back( v );
            }
            return res;
        }

        for ( const auto &i : q.items )
            res.columns.push_back( i.name );

        for ( auto r : rows )
        {
            res.rows.emplace_back();
            for ( const auto &i : q.items )
                res.rows.back().push_back( eval( *i.expr, *r ) );
        }

        return res;
    }
    catch ( const query_error & ) { return std::nullopt; }
}

int main()
{
    using namespace std::literals;
    using vr = std::vector< value >;

    database db;
    db[ "people" ] =
    {
        { { "name", "alice"s }, { "age", 31L }, { "city", "Brno"s } },
        { { "name", "bob"s },   { "age", 25L }, { "city", "Praha"s } },
        { { "name", "carol"s }, { "age", 31L }, { "city", "Praha"s } },
        { { "name", "dave"s },  { "age", 40L }, { "city", "Brno"s } },
    };
    db[ "empty" ] = {};

    auto r = query( db, "select name from people" );
    assert( r );
    assert(( r->columns == std::vector< std::string >{ "name" } ));
    assert( r->rows.size() == 4 );
    assert(( r->rows[ 1 ] == vr{ "bob"s } ));

    r = query( db, "SELECT * FROM people WHERE age > 30 AND city = 'Brno'" );
    assert(( r->columns == std::vector< std::string >{ "age", "city", "name" } ));
    assert( r->rows.size() == 2 );
    assert(( r->rows[ 0 ] == vr{ 31L, "Brno"s, "alice"s } ));
    assert(( r->rows[ 1 ] == vr{ 40L, "Brno"s, "dave"s } ));

    r = query( db, "select upper(name) as n, age * 2, age from people order by age desc" );
    assert(( r->columns == std::vector< std::string >{ "n", "col2", "age" } ));
    assert(( r->rows[ 0 ] == vr{ "DAVE"s, 80L, 40L } ));
    assert(( r->rows[ 1 ] == vr{ "ALICE"s, 62L, 31L } )); /* stable */
    assert(( r->rows[ 2 ] == vr{ "CAROL"s, 62L, 31L } ));
    assert(( r->rows[ 3 ] == vr{ "BOB"s, 50L, 25L } ));

    r = query( db, "select name + ' (' + city + ')' as who from people "
                   "where not (age < 30) order by length(name), name limit 2" );
    assert( !r ); /* ‹order by› takes a single expression */

    r = query( db, "select name + ' (' + city + ')' as who from people "
                   "where not (age < 30) order by length(name) limit 2" );
    assert( r && r->rows.size() == 2 );
    assert(( r->rows[ 0 ] == vr{ "dave (Brno)"s } ));
    assert(( r->rows[ 1 ] == vr{ "alice (Brno)"s } ));

    r = query( db, "select 'it''s', -age % 7, (1 + 2) * 3 from people limit 1" );
    assert(( r->rows[ 0 ] == vr{ "it's"s, -3L, 9L } ));

    r = query( db, "select nosuch from empty" );
    assert( r && r->rows.empty() && r->columns.size() == 1 );
    assert( query( db, "select * from empty" )->columns.empty() );

    assert( query( db, "select name from people where age > 100" )->rows.empty() );
    assert( query( db, "select 1 from people where 0 and 1 / 0" )->rows.empty() );

    assert( !query( db, "select name from nowhere" ) );
    assert( !query( db, "select nosuch from people" ) );
    assert( !query( db, "select name - 1 from people" ) );
    assert( !query( db, "select name from people where name" ) );
    assert( !query( db, "select 1 / (age - age) from people" ) );
    assert( !query( db, "select age < 'x' from people" ) );
    assert( !query( db, "select foo(name) from people" ) );
    assert( !query( db, "select name from people order by 'x' + age" ) );
    assert( !query( db, "select from people" ) );
    assert( !query( db, "select name people" ) );
    assert( !query( db, "select name from people limit" ) );
    assert( !query( db, "select 1 < 2 < 3 from people" ) );
    assert( !query( db, "select 'abc from people" ) );

    return 0;
}
