#include <cassert>
#include <string_view>
#include <string>

/* As mentioned in the introduction to the previous chapter, a lexer
 * cannot check that parentheses are balanced, because that requires
 * unbounded memory – in this case, a «stack». Write a function
 * ‹balanced› which checks that the brackets ‹()›, ‹[]› and ‹{}› in
 * the input are properly balanced and nested (‹([)]› is not). All
 * other characters are ignored. */

// bool balanced( std::string_view s );

bool balanced( std::string_view s )
{
    std::string stack; /* expected closing brackets */

    for ( char c : s )
        switch ( c )
        {
            case '(': stack.push_back( ')' ); break;
            case '[': stack.push_back( ']' ); break;
            case '{': stack.push_back( '}' ); break;
            case ')': case ']': case '}':
                if ( stack.empty() || stack.back() != c )
                    return false;
                stack.pop_back();
        }

    return stack.empty();
}

int main()
{
    assert( balanced( "" ) );
    assert( balanced( "()" ) );
    assert( balanced( "([]{})" ) );
    assert( balanced( "f(a[i], { x })" ) );
    assert( balanced( "((()))[]" ) );

    assert( !balanced( "(" ) );
    assert( !balanced( ")" ) );
    assert( !balanced( "([)]" ) );
    assert( !balanced( "(()" ) );
    assert( !balanced( "())(" ) );
    assert( !balanced( "{]" ) );

    return 0;
}
