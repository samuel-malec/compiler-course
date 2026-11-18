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

struct template_error {};

/* ─── expressions ────────────────────────────────────────────────── */

struct expr;
using expr_ptr = std::shared_ptr< const expr >;

struct expr
{
    enum kind { literal, var, member, index, unary, binary, filter } k;
    data lit = { false };
    std::string name = {};             /* variable, member, operator or filter */
    expr_ptr a = nullptr, b = nullptr;
};

/* A small lexer for the inside of a tag. */

std::vector< std::string > tag_tokens( std::string_view s )
{
    auto alpha = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_'; };
    auto digit = []( char c ) { return c >= '0' && c <= '9'; };
    std::vector< std::string > out;

    for ( std::size_t i = 0; i < s.size(); )
    {
        std::size_t start = i;
        char c = s[ i++ ];

        if ( c == ' ' || c == '\n' || c == '\t' )
            continue;

        if ( alpha( c ) )
            while ( i < s.size() && ( alpha( s[ i ] ) || digit( s[ i ] ) ) ) ++ i;
        else if ( digit( c ) )
            while ( i < s.size() && digit( s[ i ] ) ) ++ i;
        else if ( c == '"' )
        {
            /* keep the quotes and escapes; decoded by the parser */
            while ( i < s.size() && s[ i ] != '"' )
                i += s[ i ] == '\\' ? 2 : 1;
            if ( i >= s.size() )
                throw template_error();
            ++ i;
        }
        else if ( ( c == '=' || c == '!' ) && i < s.size() && s[ i ] == '=' )
            ++ i;
        else if ( std::string_view( "|.[]()+<>" ).find( c ) == s.npos )
            throw template_error();

        out.emplace_back( s.substr( start, i - start ) );
    }

    return out;
}

const std::set< std::string > reserved =
    { "and", "or", "not", "true", "false", "if", "elif", "else", "end", "for", "in" };

struct expr_parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    bool accept( std::string_view t ) { if ( peek() != t ) return false; ++ pos; return true; }
    void expect( std::string_view t ) { if ( !accept( t ) ) throw template_error(); }

    static bool is_ident( const std::string &t )
    {
        return !t.empty() && ( std::isalpha( static_cast< unsigned char >( t[ 0 ] ) ) || t[ 0 ] == '_' ) &&
               !reserved.contains( t );
    }

    std::string ident()
    {
        if ( !is_ident( peek() ) ) throw template_error();
        return toks[ pos++ ];
    }

    static expr_ptr make( expr::kind k, std::string name = "", expr_ptr a = nullptr,
                          expr_ptr b = nullptr )
    {
        return std::make_shared< expr >( k, data{ false }, name, a, b );
    }

    expr_ptr top()
    {
        auto e = disj();
        while ( accept( "|" ) )
        {
            std::string f = ident();
            expr_ptr arg;
            if ( f == "join" )
            {
                expect( "(" );
                arg = atom();
                if ( arg->k != expr::literal || !std::holds_alternative< std::string >( arg->lit.v ) )
                    throw template_error();
                expect( ")" );
            }
            e = make( expr::filter, f, e, arg );
        }
        return e;
    }

    expr_ptr disj()
    {
        auto l = conj();
        while ( accept( "or" ) ) l = make( expr::binary, "or", l, conj() );
        return l;
    }

    expr_ptr conj()
    {
        auto l = neg();
        while ( accept( "and" ) ) l = make( expr::binary, "and", l, neg() );
        return l;
    }

    expr_ptr neg()
    {
        if ( accept( "not" ) ) return make( expr::unary, "not", neg() );
        return cmp();
    }

    expr_ptr cmp()
    {
        auto l = sum();
        for ( auto op : { "==", "!=", "<", ">" } )
            if ( accept( op ) )
                return make( expr::binary, op, l, sum() );
        return l;
    }

    expr_ptr sum()
    {
        auto l = postfix();
        while ( accept( "+" ) ) l = make( expr::binary, "+", l, postfix() );
        return l;
    }

    expr_ptr postfix()
    {
        auto e = atom();
        while ( true )
            if ( accept( "." ) )
                e = make( expr::member, ident(), e );
            else if ( accept( "[" ) )
            {
                e = make( expr::index, "", e, top() );
                expect( "]" );
            }
            else
                return e;
    }

    expr_ptr atom()
    {
        if ( accept( "(" ) )
        {
            auto e = top();
            expect( ")" );
            return e;
        }

        std::string t = peek();
        auto lit = [ & ]( data d )
        {
            ++ pos;
            return std::make_shared< expr >( expr::literal, d );
        };

        if ( t == "true" || t == "false" ) return lit( data{ t == "true" } );
        if ( !t.empty() && t[ 0 ] >= '0' && t[ 0 ] <= '9' ) return lit( data{ std::stol( t ) } );
        if ( !t.empty() && t[ 0 ] == '"' )
        {
            std::string s;
            for ( std::size_t i = 1; i + 1 < t.size(); ++ i )
            {
                if ( t[ i ] == '\\' )
                {
                    ++ i;
                    if ( t[ i ] != '"' && t[ i ] != '\\' )
                        throw template_error();
                }
                s += t[ i ];
            }
            return lit( data{ s } );
        }

        return make( expr::var, ident() );
    }
};

expr_ptr parse_expr( const std::vector< std::string > &toks, std::size_t from, std::size_t to )
{
    expr_parser p{ { toks.begin() + from, toks.begin() + to } };
    auto e = p.top();
    if ( p.pos != p.toks.size() )
        throw template_error();
    return e;
}

/* ─── template structure ─────────────────────────────────────────── */

struct block;
using block_ptr = std::shared_ptr< const block >;

/* A sequence of pieces; each piece is text, an output tag, an
 * if-chain or a loop. */

struct piece
{
    enum kind { plain, output, cond, loop } k;
    std::string text = {};                                  /* text; loop variable */
    expr_ptr e = nullptr;                                   /* output; loop list */
    std::vector< std::pair< expr_ptr, block_ptr > > branches = {}; /* cond */
    block_ptr body = nullptr, otherwise = nullptr;          /* loop body; else */
};

struct block { std::vector< piece > pieces; };

/* The template is first split into a flat list of «items» (text and
 * tags); a recursive parser then builds the block structure. */

struct item
{
    char kind;                         /* 't' text, '{' output, '%' statement */
    std::string text;
    std::vector< std::string > toks;   /* for tags */
};

std::vector< item > split( std::string_view s )
{
    std::vector< item > out;
    std::string text;

    for ( std::size_t i = 0; i < s.size(); )
    {
        auto two = s.substr( i, 2 );
        if ( two == "{{" || two == "{%" || two == "{#" )
        {
            std::string_view close = two == "{{" ? "}}" : two == "{%" ? "%}" : "#}";
            auto end = s.find( close, i + 2 );
            if ( end == s.npos )
                throw template_error();

            if ( !text.empty() )
                out.push_back( { 't', text, {} } ), text.clear();

            if ( two != "{#" )
                out.push_back( { two[ 1 ], "", tag_tokens( s.substr( i + 2, end - i - 2 ) ) } );
            i = end + 2;
        }
        else
            text += s[ i++ ];
    }

    if ( !text.empty() )
        out.push_back( { 't', text, {} } );
    return out;
}

struct block_parser
{
    std::vector< item > items;
    std::size_t pos = 0;

    /* The keyword of a statement tag at ‹pos›, or "" if there is none. */
    std::string keyword() const
    {
        if ( pos >= items.size() || items[ pos ].kind != '%' || items[ pos ].toks.empty() )
            return "";
        return items[ pos ].toks[ 0 ];
    }

    /* Parse pieces until a statement tag with one of the keywords in
     * ‹stop› (or the end of input, if ‹stop› is empty). */

    block_ptr parse( const std::set< std::string > &stop )
    {
        auto b = std::make_shared< block >();

        while ( pos < items.size() )
        {
            const item &it = items[ pos ];

            if ( it.kind == 't' )
            {
                b->pieces.push_back( { piece::plain, it.text } );
                ++ pos;
                continue;
            }

            if ( it.kind == '{' )
            {
                b->pieces.push_back( { piece::output, "", parse_expr( it.toks, 0, it.toks.size() ) } );
                ++ pos;
                continue;
            }

            std::string kw = keyword();
            if ( stop.contains( kw ) )
                return b;

            if ( kw == "if" )
                b->pieces.push_back( conditional() );
            else if ( kw == "for" )
                b->pieces.push_back( loop() );
            else
                throw template_error(); /* unknown, or misplaced elif/else/end */
        }

        if ( !stop.empty() )
            throw template_error(); /* missing ‹end› */
        return b;
    }

    void expect_bare( const std::string &kw )
    {
        if ( keyword() != kw || items[ pos ].toks.size() != 1 )
            throw template_error();
        ++ pos;
    }

    piece conditional()
    {
        piece p{ piece::cond };

        while ( keyword() == "if" || keyword() == "elif" )
        {
            const auto &t = items[ pos++ ].toks;
            auto cond = parse_expr( t, 1, t.size() );
            p.branches.emplace_back( cond, parse( { "elif", "else", "end" } ) );
        }

        if ( keyword() == "else" )
        {
            expect_bare( "else" );
            p.otherwise = parse( { "end" } );
        }

        expect_bare( "end" );
        return p;
    }

    piece loop()
    {
        const auto &t = items[ pos++ ].toks;
        if ( t.size() < 4 || t[ 2 ] != "in" || !expr_parser::is_ident( t[ 1 ] ) )
            throw template_error();

        piece p{ piece::loop, t[ 1 ], parse_expr( t, 3, t.size() ) };
        p.body = parse( { "else", "end" } );

        if ( keyword() == "else" )
        {
            expect_bare( "else" );
            p.otherwise = parse( { "end" } );
        }

        expect_bare( "end" );
        return p;
    }
};

/* ─── evaluation ─────────────────────────────────────────────────── */

struct scope
{
    const object &context;
    std::vector< std::pair< std::string, const data * > > vars = {};

    const data &lookup( const std::string &name ) const
    {
        for ( auto it = vars.rbegin(); it != vars.rend(); ++ it )
            if ( it->first == name )
                return *it->second;
        auto it = context.find( name );
        if ( it == context.end() )
            throw template_error();
        return it->second;
    }
};

template< typename T >
const T &as( const data &d )
{
    if ( auto p = std::get_if< T >( &d.v ) ) return *p;
    throw template_error();
}

bool truthy( const data &d )
{
    switch ( d.v.index() )
    {
        case 0: return std::get< bool >( d.v );
        case 1: return std::get< long >( d.v ) != 0;
        case 2: return !std::get< std::string >( d.v ).empty();
        case 3: return !std::get< list >( d.v ).empty();
        default: return !std::get< object >( d.v ).empty();
    }
}

bool equal( const data &a, const data &b )
{
    if ( a.v.index() != b.v.index() ) return false;
    if ( auto l = std::get_if< list >( &a.v ) )
    {
        const auto &m = std::get< list >( b.v );
        return l->size() == m.size() && std::equal( l->begin(), l->end(), m.begin(), equal );
    }
    if ( auto o = std::get_if< object >( &a.v ) )
    {
        const auto &p = std::get< object >( b.v );
        return o->size() == p.size() &&
               std::equal( o->begin(), o->end(), p.begin(), []( auto &x, auto &y )
               {
                   return x.first == y.first && equal( x.second, y.second );
               } );
    }
    switch ( a.v.index() )
    {
        case 0:  return std::get< bool >( a.v ) == std::get< bool >( b.v );
        case 1:  return std::get< long >( a.v ) == std::get< long >( b.v );
        default: return std::get< std::string >( a.v ) == std::get< std::string >( b.v );
    }
}

std::string scalar( const data &d )
{
    if ( auto b = std::get_if< bool >( &d.v ) ) return *b ? "true" : "false";
    if ( auto i = std::get_if< long >( &d.v ) ) return std::to_string( *i );
    return as< std::string >( d );
}

data eval( const expr &e, const scope &s )
{
    switch ( e.k )
    {
        case expr::literal: return e.lit;
        case expr::var:     return s.lookup( e.name );

        case expr::member:
        {
            data base = eval( *e.a, s ); /* keep it alive: ‹o› refers into it */
            const auto &o = as< object >( base );
            auto it = o.find( e.name );
            if ( it == o.end() ) throw template_error();
            return it->second;
        }

        case expr::index:
        {
            data base = eval( *e.a, s ), key = eval( *e.b, s );
            if ( auto o = std::get_if< object >( &base.v ) )
            {
                auto it = o->find( as< std::string >( key ) );
                if ( it == o->end() ) throw template_error();
                return it->second;
            }
            const auto &l = as< list >( base );
            long i = as< long >( key );
            if ( i < 0 || i >= long( l.size() ) ) throw template_error();
            return l[ i ];
        }

        case expr::unary:
            return data{ !truthy( eval( *e.a, s ) ) };

        case expr::binary:
        {
            if ( e.name == "and" || e.name == "or" )
            {
                bool l = truthy( eval( *e.a, s ) );
                if ( l == ( e.name == "or" ) ) return data{ l };
                return data{ truthy( eval( *e.b, s ) ) };
            }

            data a = eval( *e.a, s ), b = eval( *e.b, s );

            if ( e.name == "==" ) return data{ equal( a, b ) };
            if ( e.name == "!=" ) return data{ !equal( a, b ) };

            if ( a.v.index() != b.v.index() || ( a.v.index() != 1 && a.v.index() != 2 ) )
                throw template_error();

            if ( a.v.index() == 1 )
            {
                long x = std::get< long >( a.v ), y = std::get< long >( b.v );
                if ( e.name == "<" ) return data{ x < y };
                if ( e.name == ">" ) return data{ x > y };
                return data{ x + y };
            }

            const auto &x = std::get< std::string >( a.v ), &y = std::get< std::string >( b.v );
            if ( e.name == "<" ) return data{ x < y };
            if ( e.name == ">" ) return data{ x > y };
            return data{ x + y };
        }

        case expr::filter: break;
    }

    data x = eval( *e.a, s );
    const std::string &f = e.name;

    if ( f == "upper" || f == "lower" )
    {
        std::string str = as< std::string >( x );
        for ( char &c : str )
            if ( f == "upper" && c >= 'a' && c <= 'z' ) c -= 'a' - 'A';
            else if ( f == "lower" && c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
        return data{ str };
    }

    if ( f == "length" )
    {
        if ( auto str = std::get_if< std::string >( &x.v ) ) return data{ long( str->size() ) };
        if ( auto l = std::get_if< list >( &x.v ) ) return data{ long( l->size() ) };
        return data{ long( as< object >( x ).size() ) };
    }

    if ( f == "first" || f == "last" )
    {
        const auto &l = as< list >( x );
        if ( l.empty() ) throw template_error();
        return f == "first" ? l.front() : l.back();
    }

    if ( f == "reverse" )
    {
        if ( auto str = std::get_if< std::string >( &x.v ) )
            return data{ std::string( str->rbegin(), str->rend() ) };
        auto l = as< list >( x );
        std::reverse( l.begin(), l.end() );
        return data{ l };
    }

    if ( f == "join" )
    {
        const std::string &sep = std::get< std::string >( e.b->lit.v );
        std::string out;
        bool first = true;
        for ( const auto &el : as< list >( x ) )
        {
            if ( el.v.index() != 1 && el.v.index() != 2 ) throw template_error();
            out += ( first ? "" : sep ) + scalar( el );
            first = false;
        }
        return data{ out };
    }

    throw template_error();
}

void run( const block &b, scope &s, std::string &out )
{
    for ( const auto &p : b.pieces )
        switch ( p.k )
        {
            case piece::plain:
                out += p.text;
                break;

            case piece::output:
            {
                data v = eval( *p.e, s );
                if ( v.v.index() > 2 ) throw template_error();
                out += scalar( v );
                break;
            }

            case piece::cond:
            {
                const block *chosen = p.otherwise.get();
                for ( const auto &[ c, body ] : p.branches )
                    if ( truthy( eval( *c, s ) ) )
                    {
                        chosen = body.get();
                        break;
                    }
                if ( chosen )
                    run( *chosen, s, out );
                break;
            }

            case piece::loop:
            {
                data lv = eval( *p.e, s );
                const auto &l = as< list >( lv );
                if ( l.empty() && p.otherwise )
                    run( *p.otherwise, s, out );
                for ( const auto &el : l )
                {
                    s.vars.emplace_back( p.text, &el );
                    run( *p.body, s, out );
                    s.vars.pop_back();
                }
                break;
            }
        }
}

std::optional< std::string > render( std::string_view tmpl, const data &context )
{
    try
    {
        block_parser p{ split( tmpl ) };
        block_ptr b = p.parse( {} );

        scope s{ as< object >( context ) };
        std::string out;
        run( *b, s, out );
        return out;
    }
    catch ( const template_error & ) { return std::nullopt; }
}

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
