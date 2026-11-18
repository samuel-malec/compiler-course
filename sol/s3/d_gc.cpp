#include <algorithm>
#include <cassert>
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <vector>

/* In this assignment, you will implement the memory manager of a
 * language runtime: allocation and a «generational» garbage collector
 * with a copying old generation – the design used, with many
 * refinements, by the JVM, .NET, V8 and OCaml.
 *
 * ── The heap ───────────────────────────────────────────────────────
 *
 * Values are tagged words, as in the flat memory of Chapter 12: an odd
 * word ‹2x + 1› is the integer ‹x›, an even word ‹2a› is a pointer to the
 * object at address ‹a›. An object is a header word (its number of
 * fields) followed by the fields. The heap consists of
 *
 *  • the «nursery» of ‹nursery› words, where new objects are allocated
 *    by bumping a pointer,
 *  • the «old generation»: two semispaces of ‹old› words each, one of
 *    which is in use at any time; it is also bump-allocated.
 *
 * How addresses are assigned to the three areas is up to you. The
 * roots are the public vector ‹roots› – the program's variables – which
 * the program reads and writes as it likes; the collector updates the
 * pointers in it when objects move.
 *
 * ── Collection ─────────────────────────────────────────────────────
 *
 *  • a «minor» collection copies the live objects of the nursery into
 *    the old generation (Cheney's algorithm, ‹12/p2_cheney›): the live
 *    objects are those reachable from the roots and from the fields of
 *    the «remembered» old objects, «through nursery objects only» – old
 *    objects are not traced; then the nursery and the remembered set
 *    are empty. If the free space in the old generation is smaller than
 *    the used part of the nursery, a major collection is done instead,
 *  • a «major» collection copies all live objects – of both generations,
 *    reachable from the roots – into the other semispace, which then
 *    becomes the old generation; the nursery and the remembered set are
 *    empty afterwards. If the live objects do not fit, throw
 *    ‹std::bad_alloc› (the heap is then unusable),
 *  • the «write barrier» in ‹set› adds an old object to the remembered
 *    set when a pointer to a nursery object is stored into it.
 *
 * ── Interface ──────────────────────────────────────────────────────
 *
 *  • ‹runtime( nursery, old )› – an empty heap of the given sizes,
 *  • ‹alloc( fields )› – allocate an object with the given fields in the
 *    nursery and return a pointer to it. If there is not enough room,
 *    do a minor collection first. An object which is larger than the
 *    whole nursery is allocated directly in the old generation (with a
 *    major collection first, if needed; such an object is remembered
 *    if it points into the nursery). During a collection, the pointers
 *    in ‹fields› are roots, too (and are updated),
 *  • ‹get( p, k )› and ‹set( p, k, v )› – read and write field ‹k› of
 *    the object ‹p›; ‹size( p )› – its number of fields; these throw
 *    ‹std::invalid_argument› if ‹p› is not a pointer to an object, and
 *    ‹std::out_of_range› for a bad ‹k›,
 *  • ‹minor()›, ‹major()› – collect explicitly (‹minor› may do a major
 *    collection, as described above),
 *  • ‹minors()›, ‹majors()› – the number of collections of each kind
 *    done so far (a minor collection which became a major one counts
 *    as a major one),
 *  • ‹old_used()› – the number of words in use in the old generation,
 *    ‹nursery_used()› the same for the nursery.
 *
 * The collector must not use recursion. */

using word = long;

word tag_int( long x ) { return 2 * x + 1; }
bool is_ptr( word w ) { return ( w & 1 ) == 0; }
long untag( word w ) { return w >> 1; }

// class runtime;

/* ── Tests ── */

/* A shadow copy of the object graph, kept by the tests: field 0 of
 * every object is its unique id; a reference is stored as the id of
 * the target (and an integer as itself, marked by ‹is_ref = false›). */

struct shadow_field { bool is_ref; long v; };
using shadow = std::map< long, std::vector< shadow_field > >;

/* The address space: the nursery is ‹[ 0, n )›, the two semispaces are
 * ‹[ n, n + m )› and ‹[ n + m, n + 2m )›. A copied object's header is
 * replaced by the forwarding address ‹-1 - new›. */

class runtime
{
    long _n, _m;
    std::vector< word > mem;
    long ntop = 0, obase, otop;
    std::set< long > remembered;   /* addresses of old objects */
    int _minors = 0, _majors = 0;

    static word ptr( long a ) { return 2 * a; }
    bool in_old( long a ) const { return a >= obase && a < otop; }
    long old_free() const { return _m - ( otop - obase ); }

    long object( word p ) const
    {
        if ( !is_ptr( p ) ) throw std::invalid_argument( "not a pointer" );
        long a = untag( p );
        if ( !( ( a >= 0 && a < ntop ) || in_old( a ) ) ) throw std::invalid_argument( "not an object" );
        return a;
    }

    /* copy the object at ‹a› to ‹top›, unless it has been copied
     * already; ‹limit› is the end of the target space */
    word forward( long a, long &top, long limit )
    {
        if ( mem[ a ] < 0 ) return ptr( -1 - mem[ a ] );
        long size = mem[ a ] + 1;
        if ( top + size > limit ) throw std::bad_alloc();
        std::copy( mem.begin() + a, mem.begin() + a + size, mem.begin() + top );
        mem[ a ] = -1 - top;
        top += size;
        return ptr( top - size );
    }

    template< typename Move >
    void cheney( std::vector< word > &extra, long &top, long limit, long scan, Move move,
                 const std::set< long > &more = {} )
    {
        auto fix = [ & ]( word &w ) { if ( is_ptr( w ) && move( untag( w ) ) ) w = forward( untag( w ), top, limit ); };
        for ( auto &r : roots ) fix( r );
        for ( auto &r : extra ) fix( r );
        for ( long a : more )
            for ( long k = 1; k <= mem[ a ]; ++ k ) fix( mem[ a + k ] );
        for ( ; scan < top; scan += mem[ scan ] + 1 )
            for ( long k = 1; k <= mem[ scan ]; ++ k ) fix( mem[ scan + k ] );
    }

    void major_gc( std::vector< word > &extra )
    {
        ++ _majors;
        long to = obase == _n ? _n + _m : _n, top = to;
        cheney( extra, top, to + _m, to, []( long ) { return true; } );
        obase = to, otop = top, ntop = 0;
        remembered.clear();
    }

    void minor_gc( std::vector< word > &extra )
    {
        if ( old_free() < ntop ) return major_gc( extra );
        ++ _minors;
        long top = otop;
        cheney( extra, top, obase + _m, otop, [ & ]( long a ) { return a < _n; }, remembered );
        otop = top, ntop = 0;
        remembered.clear();
    }

public:
    std::vector< word > roots;

    runtime( std::size_t nursery, std::size_t old )
        : _n( nursery ), _m( old ), mem( nursery + 2 * old ), obase( nursery ), otop( nursery ) {}

    word alloc( std::vector< word > fields )
    {
        for ( word f : fields ) if ( is_ptr( f ) ) object( f );
        long size = fields.size() + 1;

        long a;
        if ( size > _n )   /* directly into the old generation */
        {
            if ( old_free() < size ) major_gc( fields );
            if ( old_free() < size ) throw std::bad_alloc();
            a = otop, otop += size;
            for ( word f : fields )
                if ( is_ptr( f ) && untag( f ) < _n ) remembered.insert( a );
        }
        else
        {
            if ( ntop + size > _n ) minor_gc( fields );
            a = ntop, ntop += size;
        }
        mem[ a ] = fields.size();
        std::copy( fields.begin(), fields.end(), mem.begin() + a + 1 );
        return ptr( a );
    }

    std::size_t size( word p ) const { return mem[ object( p ) ]; }

    word get( word p, std::size_t k ) const
    {
        long a = object( p );
        if ( long( k ) >= mem[ a ] ) throw std::out_of_range( "no such field" );
        return mem[ a + 1 + k ];
    }

    void set( word p, std::size_t k, word v )
    {
        long a = object( p );
        if ( long( k ) >= mem[ a ] ) throw std::out_of_range( "no such field" );
        if ( is_ptr( v ) ) object( v );
        mem[ a + 1 + k ] = v;
        if ( in_old( a ) && is_ptr( v ) && untag( v ) < _n ) remembered.insert( a );
    }

    void minor() { std::vector< word > none; minor_gc( none ); }
    void major() { std::vector< word > none; major_gc( none ); }
    int minors() const { return _minors; }
    int majors() const { return _majors; }
    std::size_t old_used() const { return otop - obase; }
    std::size_t nursery_used() const { return ntop; }
};

int main()
{
    auto same_graph = []( runtime &rt, const shadow &sh, const std::vector< long > &root_ids )
    {
        std::map< long, word > seen;
        std::vector< std::pair< word, long > > todo;
        for ( std::size_t i = 0; i < root_ids.size(); ++ i ) todo.emplace_back( rt.roots[ i ], root_ids[ i ] );
        while ( !todo.empty() )
        {
            auto [ w, id ] = todo.back();
            todo.pop_back();
            if ( !is_ptr( w ) || rt.get( w, 0 ) != tag_int( id ) ) return false;
            if ( seen.contains( id ) ) { if ( seen[ id ] != w ) return false; continue; }   /* sharing */
            seen[ id ] = w;
            const auto &fs = sh.at( id );
            if ( rt.size( w ) != fs.size() + 1 ) return false;
            for ( std::size_t k = 0; k < fs.size(); ++ k )
            {
                word f = rt.get( w, k + 1 );
                if ( fs[ k ].is_ref ) todo.emplace_back( f, fs[ k ].v );
                else if ( f != tag_int( fs[ k ].v ) ) return false;
            }
        }
        return true;
    };

    {
        runtime rt( 100, 1000 );
        word a = rt.alloc( { tag_int( 1 ), tag_int( 2 ) } );
        word b = rt.alloc( { a, a } );
        rt.roots = { b, tag_int( 5 ) };
        assert( rt.nursery_used() == 6 && rt.old_used() == 0 );
        assert( rt.get( rt.get( b, 1 ), 1 ) == tag_int( 2 ) );

        rt.alloc( { tag_int( 0 ) } );   /* garbage */
        rt.minor();
        assert( rt.minors() == 1 && rt.nursery_used() == 0 && rt.old_used() == 6 );
        b = rt.roots[ 0 ];
        assert( rt.get( b, 0 ) == rt.get( b, 1 ) );   /* sharing survives */
        assert( rt.roots[ 1 ] == tag_int( 5 ) );

        /* the write barrier: a young object referenced only from an old one */
        word y = rt.alloc( { tag_int( 42 ) } );
        rt.set( b, 0, y );
        rt.minor();
        assert( rt.old_used() == 8 && rt.get( rt.get( rt.roots[ 0 ], 0 ), 0 ) == tag_int( 42 ) );

        /* old garbage stays until a major collection */
        rt.set( rt.roots[ 0 ], 0, tag_int( 0 ) );
        rt.set( rt.roots[ 0 ], 1, tag_int( 0 ) );
        rt.minor();
        assert( rt.old_used() == 8 );
        rt.major();
        assert( rt.majors() == 1 && rt.old_used() == 3 );

        bool caught = false;
        try { rt.get( tag_int( 3 ), 0 ); } catch ( const std::invalid_argument & ) { caught = true; }
        assert( caught );
        caught = false;
        try { rt.get( rt.roots[ 0 ], 2 ); } catch ( const std::out_of_range & ) { caught = true; }
        assert( caught );
    }

    {
        /* allocation triggers collections; the fields of the new object
         * are roots during them */
        runtime rt( 30, 300 );
        rt.roots = { tag_int( 0 ) };
        for ( int i = 0; i < 1000; ++ i )
        {
            word cell = rt.alloc( { tag_int( i ), rt.roots[ 0 ] } );
            rt.roots[ 0 ] = cell;
            if ( i % 20 == 19 ) rt.roots[ 0 ] = tag_int( 0 );   /* drop the list */
        }
        assert( rt.minors() > 50 && rt.majors() > 0 );

        /* an object larger than the nursery */
        std::vector< word > big( 40, tag_int( 7 ) );
        big[ 0 ] = rt.alloc( { tag_int( 1 ) } );
        word w = rt.alloc( big );
        assert( rt.size( w ) == 40 && rt.get( rt.get( w, 0 ), 0 ) == tag_int( 1 ) );

        /* out of memory */
        bool caught = false;
        try
        {
            for ( int i = 0; i < 1000; ++ i ) rt.roots.push_back( rt.alloc( { tag_int( i ) } ) );
        }
        catch ( const std::bad_alloc & ) { caught = true; }
        assert( caught );
    }

    {
        /* a random mutator, checked against a shadow copy of the graph */
        runtime rt( 200, 1000 );
        shadow sh;
        std::vector< long > root_ids;
        long next_id = 0;
        int explicit_majors = 0;
        unsigned seed = 3;
        auto rnd = [ & ]( unsigned m ) { seed = seed * 1103515245 + 12345; return ( seed >> 8 ) % m; };

        for ( int step = 0; step < 20000; ++ step )
        {
            int what = rnd( 10 );
            /* a random reachable object: a root, then a few random fields */
            auto pick = [ & ]( word &w, long &id ) -> bool
            {
                if ( root_ids.empty() ) return false;
                std::size_t r = rnd( root_ids.size() );
                w = rt.roots[ r ], id = root_ids[ r ];
                for ( int d = rnd( 4 ); d > 0; -- d )
                {
                    auto &fs = sh[ id ];
                    std::size_t k = rnd( fs.size() );
                    if ( !fs[ k ].is_ref ) break;
                    w = rt.get( w, k + 1 ), id = fs[ k ].v;
                }
                return true;
            };

            if ( what < 5 || root_ids.size() < 3 )   /* allocate */
            {
                std::vector< word > fields{ tag_int( next_id ) };
                std::vector< shadow_field > sf;
                for ( int k = 1 + rnd( 4 ); k > 0; -- k )
                {
                    word w; long id;
                    if ( rnd( 2 ) && pick( w, id ) ) fields.push_back( w ), sf.push_back( { true, id } );
                    else { long x = rnd( 100 ); fields.push_back( tag_int( x ) ), sf.push_back( { false, x } ); }
                }
                word obj = rt.alloc( fields );
                sh[ next_id ] = sf;
                if ( root_ids.size() < 8 ) rt.roots.push_back( obj ), root_ids.push_back( next_id );
                else { std::size_t r = rnd( root_ids.size() ); rt.roots[ r ] = obj, root_ids[ r ] = next_id; }
                ++ next_id;
            }
            else if ( what < 9 )   /* store into a field */
            {
                word w, v; long id, vid;
                if ( !pick( w, id ) ) continue;
                std::size_t k = rnd( sh[ id ].size() );
                if ( rnd( 3 ) && pick( v, vid ) ) rt.set( w, k + 1, v ), sh[ id ][ k ] = { true, vid };
                else rt.set( w, k + 1, tag_int( 7 ) ), sh[ id ][ k ] = { false, 7 };
            }
            else if ( rnd( 50 ) == 0 ) rt.major(), ++ explicit_majors;

            if ( step % 500 == 0 ) assert( same_graph( rt, sh, root_ids ) );
        }
        assert( same_graph( rt, sh, root_ids ) );
        /* most collections are minor; some major ones were needed */
        assert( rt.majors() > explicit_majors && rt.minors() > 20 * ( rt.majors() - explicit_majors ) );
    }

    return 0;
}
