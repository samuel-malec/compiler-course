#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <set>

/* Every exercise in this chapter came with a grammar written in EBNF.
 * EBNF is, of course, a language too – so let's write a parser for
 * it. The grammar of (our subset of) EBNF, written in EBNF:
 *
 *     grammar  = { rule } ;
 *     rule     = ident, '=', alt, ';' ;
 *     alt      = seq, { '|', seq } ;
 *     seq      = term, { ',', term } ;
 *     term     = ident | terminal
 *              | '[', alt, ']' | '{', alt, '}' | '(', alt, ')' ;
 *     ident    = letter, { letter | digit | '_' } ;
 *     terminal = "'", char, { char }, "'" ;
 *
 * Here, ‹letter› is an ASCII letter, ‹digit› is a decimal digit and
 * ‹char› is any character except ‹'›. Whitespace (spaces and
 * newlines) may appear between any two tokens.
 *
 * Write a function ‹parse_ebnf› which parses a grammar and returns
 * one string per rule (in the order of the input), giving the
 * structure of the rule in this canonical form:
 *
 *     name = TREE
 *
 * where ‹TREE› is an S-expression built as follows:
 *
 *  • an identifier is printed as itself, a terminal in single quotes,
 *  • an ‹alt› with 2 or more alternatives is printed as ‹(alt X Y …)›,
 *  • a ‹seq› with 2 or more terms is printed as ‹(seq X Y …)›,
 *  • an ‹alt› with a single alternative, or a ‹seq› with a single
 *    term, is printed as just that alternative or term (so
 *    parentheses used for grouping disappear: ‹( ( a ) )› is ‹a›),
 *  • ‹[ X ]› is printed as ‹(opt X)› and ‹{ X }› as ‹(rep X)›.
 *
 * Return ‹std::nullopt› if the input does not match the grammar, or
 * if the same nonterminal is defined by more than one rule. */

// std::optional< std::vector< std::string > > parse_ebnf( std::string_view );

/* We print directly while parsing; each parsing function returns the
 * printed form of what it parsed, or ‹std::nullopt›. */

struct ebnf_parser
{
    std::string_view s;
    std::size_t pos = 0;

    static bool letter( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ); }
    static bool digit( char c ) { return c >= '0' && c <= '9'; }

    void ws() { while ( pos < s.size() && ( s[ pos ] == ' ' || s[ pos ] == '\n' ) ) ++ pos; }

    bool accept( char c )
    {
        ws();
        if ( pos < s.size() && s[ pos ] == c )
            return ++ pos, true;
        return false;
    }

    std::optional< std::string > ident()
    {
        ws();
        std::size_t start = pos;
        if ( pos >= s.size() || !letter( s[ pos ] ) )
            return std::nullopt;
        while ( pos < s.size() && ( letter( s[ pos ] ) || digit( s[ pos ] ) || s[ pos ] == '_' ) )
            ++ pos;
        return std::string( s.substr( start, pos - start ) );
    }

    /* Join a list: a single element is printed as itself. */
    static std::string group( std::string tag, const std::vector< std::string > &xs )
    {
        if ( xs.size() == 1 )
            return xs[ 0 ];
        std::string out = "(" + tag;
        for ( const auto &x : xs )
            out += " " + x;
        return out + ")";
    }

    std::optional< std::string > alt()
    {
        std::vector< std::string > alts;
        do
        {
            auto x = seq();
            if ( !x ) return std::nullopt;
            alts.push_back( *x );
        } while ( accept( '|' ) );
        return group( "alt", alts );
    }

    std::optional< std::string > seq()
    {
        std::vector< std::string > terms;
        do
        {
            auto x = term();
            if ( !x ) return std::nullopt;
            terms.push_back( *x );
        } while ( accept( ',' ) );
        return group( "seq", terms );
    }

    std::optional< std::string > bracketed( char close, std::string tag )
    {
        auto inner = alt();
        if ( !inner || !accept( close ) )
            return std::nullopt;
        return tag.empty() ? *inner : "(" + tag + " " + *inner + ")";
    }

    std::optional< std::string > term()
    {
        if ( accept( '[' ) ) return bracketed( ']', "opt" );
        if ( accept( '{' ) ) return bracketed( '}', "rep" );
        if ( accept( '(' ) ) return bracketed( ')', "" );

        if ( accept( '\'' ) )
        {
            auto end = s.find( '\'', pos );
            if ( end == s.npos || end == pos )
                return std::nullopt;
            auto text = s.substr( pos - 1, end - pos + 2 );
            pos = end + 1;
            return std::string( text );
        }

        return ident();
    }

    std::optional< std::vector< std::string > > grammar()
    {
        std::vector< std::string > out;
        std::set< std::string > defined;

        while ( ws(), pos < s.size() )
        {
            auto name = ident();
            if ( !name || !defined.insert( *name ).second || !accept( '=' ) )
                return std::nullopt;
            auto body = alt();
            if ( !body || !accept( ';' ) )
                return std::nullopt;
            out.push_back( *name + " = " + *body );
        }

        return out;
    }
};

std::optional< std::vector< std::string > > parse_ebnf( std::string_view s )
{
    return ebnf_parser{ s }.grammar();
}

int main()
{
    using v = std::vector< std::string >;

    assert(( parse_ebnf( "" ) == v{} ));
    assert(( parse_ebnf( "a = b ;" ) == v{ "a = b" } ));
    assert(( parse_ebnf( "a='x';" ) == v{ "a = 'x'" } ));
    assert(( parse_ebnf( "a = b, c | d ;" ) == v{ "a = (alt (seq b c) d)" } ));
    assert(( parse_ebnf( "a = b, ( c | d ) ;" ) == v{ "a = (seq b (alt c d))" } ));
    assert(( parse_ebnf( "a = ( ( b ) ) ;" ) == v{ "a = b" } ));
    assert(( parse_ebnf( "a = [ b ], { c, d } ;" ) ==
             v{ "a = (seq (opt b) (rep (seq c d)))" } ));

    assert(( parse_ebnf( "expr = term, { ( '+' | '-' ), term } ;" ) ==
             v{ "expr = (seq term (rep (seq (alt '+' '-') term)))" } ));

    assert(( parse_ebnf( "atom   = number\n"
                         "       | '(', expr, ')' ;\n"
                         "number = digit, { digit } ;\n" ) ==
             v{ "atom = (alt number (seq '(' expr ')'))",
                "number = (seq digit (rep digit))" } ));

    assert(( parse_ebnf( "op = '<=' | '>=' | 'a b' ;" ) ==
             v{ "op = (alt '<=' '>=' 'a b')" } ));
    assert(( parse_ebnf( "x_1 = Y2 ;" ) == v{ "x_1 = Y2" } ));

    assert( !parse_ebnf( "a = b" ) );
    assert( !parse_ebnf( "a = ;" ) );
    assert( !parse_ebnf( "a = b c ;" ) );
    assert( !parse_ebnf( "a = 'b ;" ) );
    assert( !parse_ebnf( "a = '' ;" ) );
    assert( !parse_ebnf( "a = [ b ;" ) );
    assert( !parse_ebnf( "a = ( b ] ;" ) );
    assert( !parse_ebnf( "= b ;" ) );
    assert( !parse_ebnf( "1a = b ;" ) );
    assert( !parse_ebnf( "a = b, ;" ) );
    assert( !parse_ebnf( "a = b | ;" ) );
    assert( !parse_ebnf( "a = b ; a = c ;" ) );

    return 0;
}
