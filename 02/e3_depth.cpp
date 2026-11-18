#include <cassert>
#include <optional>
#include <string_view>

/* An S-expression is either an «atom» (a non-empty sequence of
 * lowercase letters and digits), or a parenthesised list of zero or
 * more S-expressions, separated by spaces:
 *
 *     sexpr = atom | '(', [ sexpr, { ' ', sexpr } ], ')' ;
 *     atom  = achar, { achar } ;
 *
 * where ‹achar› is a lowercase letter or a digit. Note that the
 * grammar is strict about spaces: there must be «exactly one» space
 * between the elements of a list, and none after ‹(› or before ‹)›.
 *
 * Write a function ‹depth› which checks that the entire input is a
 * single ‹sexpr›, and returns its «nesting depth»: 0 for an atom, and
 * 1 + the maximum depth of the elements for a list (with the maximum
 * of an empty list being 0). Return ‹std::nullopt› for invalid
 * inputs. Use recursive descent. */

// std::optional< int > depth( std::string_view s );

int main()
{
    assert( depth( "a" ) == 0 );
    assert( depth( "abc12" ) == 0 );
    assert( depth( "()" ) == 1 );
    assert( depth( "(a b c)" ) == 1 );
    assert( depth( "(a (b c) ((d)))" ) == 3 );
    assert( depth( "(() ())" ) == 2 );
    assert( depth( "((((x))))" ) == 4 );

    assert( !depth( "" ) );
    assert( !depth( "(" ) );
    assert( !depth( "(a))" ) );
    assert( !depth( "( a)" ) );
    assert( !depth( "(a )" ) );
    assert( !depth( "(a  b)" ) );
    assert( !depth( "a b" ) );
    assert( !depth( "(A)" ) );
    assert( !depth( "(a)(b)" ) );

    return 0;
}
