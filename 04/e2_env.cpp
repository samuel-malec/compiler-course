#include <cassert>
#include <memory>
#include <optional>
#include <string>

/* Write a class ‹environment›, which represents one «frame» of a
 * chained environment (see ‹d1_eval›), with mutable bindings of
 * names to ‹long› values. A frame optionally has a parent frame,
 * given to the constructor as a ‹std::shared_ptr< environment >›
 * (the default is ‹nullptr›, i.e. no parent). It should provide these
 * methods:
 *
 *  • ‹define( name, value )› – create a binding in «this» frame; if
 *    the frame already has a binding for ‹name›, do nothing and
 *    return ‹false›, otherwise return ‹true›,
 *  • ‹lookup( name )› – return the value bound to ‹name› in the
 *    nearest frame (this one, its parent, the parent's parent, …)
 *    which has a binding for it, or ‹std::nullopt› if there is none,
 *  • ‹assign( name, value )› – change the value of the nearest
 *    existing binding of ‹name›, and return ‹true›; if there is no
 *    binding, return ‹false›. */

// class environment;

int main()
{
    auto global = std::make_shared< environment >();
    assert( global->define( "x", 1 ) );
    assert( !global->define( "x", 2 ) );
    assert( global->lookup( "x" ) == 1 );
    assert( !global->lookup( "y" ) );

    auto inner = std::make_shared< environment >( global );
    assert( inner->lookup( "x" ) == 1 );
    assert( inner->define( "x", 10 ) );   /* shadows the outer x */
    assert( inner->lookup( "x" ) == 10 );
    assert( global->lookup( "x" ) == 1 );

    assert( inner->define( "y", 5 ) );
    assert( !global->lookup( "y" ) );

    auto innermost = std::make_shared< environment >( inner );
    assert( innermost->assign( "x", 20 ) ); /* the nearest x: inner's */
    assert( inner->lookup( "x" ) == 20 );
    assert( global->lookup( "x" ) == 1 );
    assert( !innermost->assign( "z", 0 ) );

    assert( global->define( "g", 7 ) );
    assert( innermost->lookup( "g" ) == 7 );
    assert( innermost->assign( "g", 8 ) );
    assert( global->lookup( "g" ) == 8 );

    return 0;
}
