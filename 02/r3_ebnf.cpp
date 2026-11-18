#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
