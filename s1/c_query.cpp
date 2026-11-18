#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

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
