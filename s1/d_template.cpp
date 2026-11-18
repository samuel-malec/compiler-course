#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/* «Template engines» (Jinja, Mustache, Liquid, …) generate text –
 * web pages, configuration files, e-mails – from a «template» with
 * embedded expressions and control structures, and a piece of
 * «data». They are small programming languages, and implementing one
 * needs everything from block 1: a lexer with modes (text vs. tags),
 * a parser which builds a tree of nested blocks, an expression
 * parser, and an evaluator.
 *
 * The data is a JSON-like tree of values: */

struct data;
using list = std::vector< data >;
using object = std::map< std::string, data >;

struct data
{
    std::variant< bool, long, std::string, list, object > v;
};

/* A template is text, interspersed with «tags»:
 *
 *  • ‹{{ expr }}› – outputs the value of the expression: an integer
 *    in decimal, a string as is, a boolean as ‹true› or ‹false›; it is
 *    an error to output a list or an object,
 *  • ‹{% if expr %}›, optionally followed by any number of
 *    ‹{% elif expr %}›, optionally followed by ‹{% else %}›, and
 *    finally ‹{% end %}› – outputs the first branch whose condition
 *    is «truthy» (or the ‹else› branch if none is),
 *  • ‹{% for x in expr %} … {% end %}› – the expression must be a
 *    list; outputs the body once for each element, with the variable
 *    ‹x› bound to the element; the loop can have an ‹{% else %}›
 *    branch (before ‹{% end %}›), which is output if the list is
 *    empty,
 *  • ‹{# … #}› – a comment, which outputs nothing.
 *
 * Everything outside of tags is copied to the output as is
 * (including a ‹{› which does not start a tag). Inside tags, tokens
 * may be separated by whitespace. Blocks nest arbitrarily.
 *
 * The syntax of expressions:
 *
 *     expr    = disj, { '|', filter } ;
 *     disj    = conj, { 'or', conj } ;
 *     conj    = neg, { 'and', neg } ;
 *     neg     = 'not', neg | cmp ;
 *     cmp     = sum, [ ( '==' | '!=' | '<' | '>' ), sum ] ;
 *     sum     = postfix, { '+', postfix } ;
 *     postfix = atom, { '.', ident | '[', expr, ']' } ;
 *     atom    = integer | string | 'true' | 'false' | ident
 *             | '(', expr, ')' ;
 *     filter  = ident | 'join', '(', string, ')' ;
 *
 * Identifiers are letters, digits and ‹_›, starting with a letter or
 * ‹_›; integers are sequences of digits; strings are enclosed in ‹"›,
 * with escapes ‹\"› and ‹\\›. The words ‹and›, ‹or›, ‹not›, ‹true›,
 * ‹false›, ‹if›, ‹elif›, ‹else›, ‹end›, ‹for› and ‹in› are reserved.
 *
 * The meaning:
 *
 *  • an identifier is a variable: the innermost enclosing loop
 *    variable of that name, or else the member of that name of the
 *    «context» object (the second argument of ‹render›),
 *  • ‹a.name› is the member ‹name› of the object ‹a›; ‹a[i]› is the
 *    element at (0-based) index ‹i› of the list ‹a›, or the member
 *    ‹i› of the object ‹a› if ‹i› is a string,
 *  • ‹+› adds two integers or concatenates two strings,
 *  • ‹==› and ‹!=› compare any two values (structurally; values of
 *    different types are never equal), ‹<› and ‹>› compare two integers
 *    or two strings,
 *  • a value is «truthy» if it is ‹true›, a non-zero integer, or a
 *    non-empty string, list or object; ‹and›, ‹or› and ‹not› work on
 *    truthiness, return booleans, and do not evaluate the right
 *    operand of ‹and› / ‹or› when the left one decides the result,
 *  • the filters: ‹upper› and ‹lower› convert ASCII letters in a
 *    string; ‹length› is the number of characters of a string or of
 *    elements of a list or an object; ‹first› and ‹last› give the
 *    first and last element of a non-empty list; ‹reverse› reverses a
 *    string or a list; ‹join( sep )› turns a list of strings and
 *    integers into a string, with ‹sep› in between the elements.
 *
 * Write a function ‹render( template, context )› which returns the
 * output, or ‹std::nullopt› on error. The «whole» template must be
 * syntactically valid (even the parts which are never output), so
 * parse it completely before producing any output. Other errors – a
 * missing variable or member, an index out of range, a type error,
 * an unknown filter – only count if the offending expression is
 * actually evaluated. It is also an error if ‹context› is not an
 * object. */

// std::optional< std::string > render( std::string_view tmpl, const data &context );

int main()
{
    using namespace std::literals;

    auto I = []( long x ) { return data{ x }; };
    auto S = []( std::string s ) { return data{ s }; };
    auto L = []( list l ) { return data{ l }; };

    data ctx{ object
    {
        { "name", S( "World" ) },
        { "n", I( 3 ) },
        { "items", L( { S( "apple" ), S( "pear" ), S( "plum" ) } ) },
        { "empty", L( {} ) },
        { "user", data{ object{ { "first", S( "Ada" ) }, { "age", I( 36 ) },
                                { "admin", data{ true } } } } },
        { "matrix", L( { L( { I( 1 ), I( 2 ) } ), L( { I( 3 ), I( 4 ) } ) } ) },
    } };

    auto r = [ & ]( std::string_view t ) { return render( t, ctx ); };

    assert( r( "" ) == "" );
    assert( r( "plain { text }" ) == "plain { text }" );
    assert( r( "Hello, {{ name }}!" ) == "Hello, World!" );
    assert( r( "{{n+1}} {{ \"a\" + \"b\" }} {{ n == 3 }}" ) == "4 ab true" );
    assert( r( "{{ user.first }} is {{ user[\"age\"] }}" ) == "Ada is 36" );
    assert( r( "{{ matrix[1][0] }}{{ items[2] }}" ) == "3plum" );
    assert( r( "a{# a comment {{ with }} stuff #}b" ) == "ab" );

    assert( r( "{% if n > 5 %}big{% elif n > 2 %}medium{% else %}small{% end %}" ) == "medium" );
    assert( r( "{% if empty %}x{% else %}none{% end %}" ) == "none" );
    assert( r( "{% if user.admin and not empty %}A{% end %}." ) == "A." );

    assert( r( "{% for i in items %}<{{ i }}>{% end %}" ) == "<apple><pear><plum>" );
    assert( r( "{% for i in empty %}x{% else %}nothing{% end %}" ) == "nothing" );
    assert( r( "{% for row in matrix %}{% for x in row %}{{ x }}{% end %};{% end %}" ) == "12;34;" );
    assert( r( "{% for name in items %}{{ name }} {% end %}{{ name }}" ) == "apple pear plum World" );

    assert( r( "{{ name | upper }} {{ name | lower | reverse }}" ) == "WORLD dlrow" );
    assert( r( "{{ items | length }} {{ items | first }} {{ items | last | upper }}" ) == "3 apple PLUM" );
    assert( r( "{{ items | reverse | join( \", \" ) }}" ) == "plum, pear, apple" );
    assert( r( "{{ matrix | first | join(\"+\") }}" ) == "1+2" );
    assert( r( "{{ \"say \\\"hi\\\"\" }}" ) == "say \"hi\"" );
    assert( r( "{% if 0 and nosuch %}x{% end %}{% if 1 or nosuch %}y{% end %}" ) == "y" );
    assert( r( "{% if 0 %}{{ nosuch }}{% end %}ok" ) == "ok" );

    assert( !r( "{{ nosuch }}" ) );
    assert( !r( "{{ user.last }}" ) );
    assert( !r( "{{ items[3] }}" ) );
    assert( !r( "{{ items }}" ) );
    assert( !r( "{{ n + name }}" ) );
    assert( !r( "{{ n | upper }}" ) );
    assert( !r( "{{ empty | first }}" ) );
    assert( !r( "{{ name | frobnicate }}" ) );
    assert( !r( "{% for x in n %}{% end %}" ) );
    assert( !r( "{{ name" ) );
    assert( !r( "{% if n %}" ) );
    assert( !r( "{% end %}" ) );
    assert( !r( "{% if n %}{% else %}{% elif n %}{% end %}" ) );
    assert( !r( "{% if 0 %}{{ ( }}{% end %}" ) );
    assert( !r( "{% while n %}{% end %}" ) );
    assert( !r( "{# unterminated" ) );
    assert( !render( "x", I( 1 ) ) );

    return 0;
}
