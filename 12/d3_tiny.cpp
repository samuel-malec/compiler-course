#include <algorithm>
#include <cassert>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/* This demo is a review of the whole course: a complete (if tiny)
 * implementation of a programming language, with every stage we have
 * studied – reading the source text, checking names, compiling to
 * bytecode, a virtual machine with frames, and a garbage collected
 * heap. The language is a small Lisp for processing lists:
 *
 *     program := form*
 *     form    := (define (name param*) expr) | expr
 *     expr    := integer | nil | name
 *              | (if expr expr expr) | (let name expr expr)
 *              | (begin expr+) | (print expr)
 *              | (+ expr expr) | (- expr expr) | (* expr expr)
 *              | (< expr expr) | (= expr expr)
 *              | (cons expr expr) | (car expr) | (cdr expr) | (pair? expr)
 *              | (name expr*)
 *
 * The top-level expressions are evaluated in order. ‹nil› is the
 * integer 0, which is also ‘false’ (in ‹if›, any other value is true);
 * ‹=› compares integers by value and pairs by identity. Functions are
 * global and may be (mutually) recursive. ‹(print e)› prints the
 * integer ‹e› and returns it.
 *
 * The program is given as a single S-expression – a list of forms –
 * so the S-expression reader (Chapter 2) does the parsing. */

/* The programs in this exercise are written as S-expressions, so
 * that we can concentrate on evaluation instead of parsing. The
 * function ‹read› (provided) turns the text into a tree of ‹sexp›
 * nodes: each node is either an «atom» – a number, a name, a string
 * literal «including» its quotes (no escapes), or anything else
 * without spaces and parentheses – or a «list» of nodes. ‹offset›
 * is the position of the first character of the node in the input.
 * The function ‹write› does the opposite of ‹read›. If the input is
 * malformed, ‹read› throws ‹std::invalid_argument›. */

struct sexp
{
    std::string atom;           /* non-empty for atoms */
    std::vector< sexp > items;  /* elements of a list */
    std::size_t offset = 0;

    bool is_atom() const { return !atom.empty(); }
    bool is_list() const { return atom.empty(); }
};

void skip_space( std::string_view s, std::size_t &pos )
{
    while ( pos < s.size() && ( s[ pos ] == ' ' || s[ pos ] == '\n' ) )
        ++ pos;
}

sexp read_at( std::string_view s, std::size_t &pos )
{
    skip_space( s, pos );
    sexp out{ "", {}, pos };

    if ( pos >= s.size() || s[ pos ] == ')' )
        throw std::invalid_argument( "unexpected end of list or input" );

    if ( s[ pos ] == '(' )
    {
        for ( ++ pos; skip_space( s, pos ), pos >= s.size() || s[ pos ] != ')'; )
            out.items.push_back( read_at( s, pos ) );
        ++ pos;
        return out;
    }

    std::size_t start = pos;

    if ( s[ pos ] == '"' )
    {
        pos = s.find( '"', pos + 1 );
        if ( pos == s.npos )
            throw std::invalid_argument( "unterminated string" );
        ++ pos;
    }
    else
        while ( pos < s.size() && std::string_view( " \n()" ).find( s[ pos ] ) == s.npos )
            ++ pos;

    out.atom = s.substr( start, pos - start );
    return out;
}

sexp read( std::string_view s )
{
    std::size_t pos = 0;
    sexp e = read_at( s, pos );
    skip_space( s, pos );
    if ( pos != s.size() )
        throw std::invalid_argument( "trailing input" );
    return e;
}

std::string write( const sexp &e )
{
    if ( e.is_atom() )
        return e.atom;

    std::string out;
    for ( const auto &i : e.items )
        out += ( out.empty() ? "" : " " ) + write( i );
    return "(" + out + ")";
}

/* The heap is the flat memory of ‹d2_copying›, with tagged words. A
 * pair is an object with two fields. All values in the VM – in local
 * slots, on operand stacks, and in pairs – are tagged words, so the
 * collector can tell pointers from integers without any help from the
 * compiler (compare ‹r3_stackmaps›). */

/* Flat memory (see ‹d2_copying›). A ‹space› is a vector of words with
 * an allocation pointer ‹top›: the words below ‹top› are in use, and
 * allocation simply moves ‹top› up («bump allocation»). An object at
 * address ‹a› is a «header» word ‹mem[ a ]›, which holds the number of
 * fields ‹n›, followed by the fields ‹mem[ a + 1 ]› … ‹mem[ a + n ]›.
 *
 * Every field is a «tagged» word: an odd word ‹2x + 1› is the integer
 * ‹x›, an even word ‹2a› is a pointer to the object at address ‹a› (this
 * is how OCaml and many implementations of Lisp tell pointers from
 * integers). A collector can thus find all the pointers in an object
 * without knowing anything about its type, and the space is
 * «parsable»: walking from address 0, header by header, visits every
 * object.
 *
 * ‹alloc› returns the address of a new object, or ‹std::nullopt› if
 * there is not enough room. ‹show› prints the structure reachable from a
 * word as nested lists (e.g. ‹(1 (2 3))›), down to the given depth (‹…›
 * below it), with ‹?› for a pointer to something which is not an
 * object. */

using word = long;

word tag_int( long x ) { return 2 * x + 1; }
word tag_ptr( long a ) { return 2 * a; }
bool is_ptr( word w ) { return ( w & 1 ) == 0; }
long untag( word w ) { return w >> 1; }

struct space
{
    std::vector< word > mem;
    std::size_t top = 0;

    explicit space( std::size_t size ) : mem( size ) {}

    std::optional< long > alloc( const std::vector< word > &fields )
    {
        if ( top + 1 + fields.size() > mem.size() ) return std::nullopt;
        long a = top;
        mem[ top++ ] = fields.size();
        for ( auto w : fields ) mem[ top++ ] = w;
        return a;
    }
};

std::string show( const space &s, word w, int depth = 20 )
{
    if ( !is_ptr( w ) ) return std::to_string( untag( w ) );
    long a = untag( w );
    if ( a < 0 || a >= long( s.top ) || s.mem[ a ] < 0 || a + s.mem[ a ] >= long( s.top ) ) return "?";
    if ( depth == 0 ) return "…";
    std::string out = "(";
    for ( long k = 0; k < s.mem[ a ]; ++ k )
        out += ( k ? " " : "" ) + show( s, s.mem[ a + 1 + k ], depth - 1 );
    return out + ")";
}

/* The bytecode. Like the VM of Chapter 10, functions have their own
 * code, slots and operand stacks; values are tagged words. */

enum class op { push, load, store, pop, add, sub, mul, lt, eq, jz, jump, cons, car, cdr, pairp, call, ret, print };

struct instr { op o; long arg = 0; };

struct function
{
    std::string name;
    int params = 0, slots = 0;
    std::vector< instr > code = {};
};

using module = std::vector< function >;   /* function 0 is the top level */

/* The compiler. Names are resolved at compile time: a variable is a
 * slot number, a function an index into the module. Errors in the
 * program (unknown names, wrong arity, malformed forms) are reported
 * by throwing ‹std::invalid_argument›. */

struct compiler
{
    module m{ { "main" } };
    std::map< std::string, int > index;   /* function name → index */

    using env = std::map< std::string, int >;   /* variable → slot */

    [[noreturn]] void error( const std::string &what ) { throw std::invalid_argument( what ); }

    static bool is_number( const std::string &s )
    {
        std::size_t k = !s.empty() && s[ 0 ] == '-' ? 1 : 0;
        return s.size() > k && s.find_first_not_of( "0123456789", k ) == s.npos;
    }

    void emit( function &f, op o, long arg = 0 ) { f.code.push_back( { o, arg } ); }

    void expr( function &f, const sexp &e, const env &vars )
    {
        if ( e.is_atom() )
        {
            if ( is_number( e.atom ) ) return emit( f, op::push, tag_int( std::stol( e.atom ) ) );
            if ( e.atom == "nil" ) return emit( f, op::push, tag_int( 0 ) );
            auto v = vars.find( e.atom );
            if ( v == vars.end() ) error( "unknown variable " + e.atom );
            return emit( f, op::load, v->second );
        }

        if ( e.items.empty() || !e.items[ 0 ].is_atom() ) error( "bad expression" );
        const std::string &head = e.items[ 0 ].atom;
        std::size_t argc = e.items.size() - 1;
        auto arg = [ & ]( std::size_t k ) -> const sexp & { return e.items[ k ]; };
        auto need = [ & ]( std::size_t n ) { if ( argc != n ) error( "wrong number of operands to " + head ); };

        static const std::map< std::string, op > binary =
            { { "+", op::add }, { "-", op::sub }, { "*", op::mul }, { "<", op::lt }, { "=", op::eq },
              { "cons", op::cons } };
        static const std::map< std::string, op > unary =
            { { "car", op::car }, { "cdr", op::cdr }, { "pair?", op::pairp }, { "print", op::print } };

        if ( auto b = binary.find( head ); b != binary.end() )
        {
            need( 2 );
            expr( f, arg( 1 ), vars );
            expr( f, arg( 2 ), vars );
            return emit( f, b->second );
        }
        if ( auto u = unary.find( head ); u != unary.end() )
        {
            need( 1 );
            expr( f, arg( 1 ), vars );
            return emit( f, u->second );
        }
        if ( head == "if" )   /* the jumps are backpatched (Chapter 10) */
        {
            need( 3 );
            expr( f, arg( 1 ), vars );
            std::size_t jz = f.code.size();
            emit( f, op::jz );
            expr( f, arg( 2 ), vars );
            std::size_t jump = f.code.size();
            emit( f, op::jump );
            f.code[ jz ].arg = f.code.size();
            expr( f, arg( 3 ), vars );
            f.code[ jump ].arg = f.code.size();
            return;
        }
        if ( head == "let" )
        {
            need( 3 );
            if ( !arg( 1 ).is_atom() ) error( "bad let" );
            expr( f, arg( 2 ), vars );
            env inner = vars;
            inner[ arg( 1 ).atom ] = f.slots;
            emit( f, op::store, f.slots++ );
            return expr( f, arg( 3 ), inner );
        }
        if ( head == "begin" )
        {
            if ( argc == 0 ) error( "empty begin" );
            for ( std::size_t k = 1; k <= argc; ++ k )
            {
                expr( f, arg( k ), vars );
                if ( k < argc ) emit( f, op::pop );
            }
            return;
        }

        auto fn = index.find( head );
        if ( fn == index.end() ) error( "unknown function " + head );
        need( m[ fn->second ].params );
        for ( std::size_t k = 1; k <= argc; ++ k )
            expr( f, arg( k ), vars );
        emit( f, op::call, fn->second );
    }

    static bool is_define( const sexp &form )
    {
        return form.is_list() && !form.items.empty() && form.items[ 0 ].atom == "define";
    }

    module compile( const sexp &program )
    {
        if ( !program.is_list() ) error( "a program is a list of forms" );

        /* pass 1: collect the functions, so that they can call each
         * other regardless of the order of definitions */
        for ( const auto &form : program.items )
        {
            if ( !is_define( form ) ) continue;
            if ( form.items.size() != 3 || !form.items[ 1 ].is_list() || form.items[ 1 ].items.empty() )
                error( "bad define" );
            const auto &sig = form.items[ 1 ].items;
            for ( const auto &p : sig ) if ( !p.is_atom() ) error( "bad define" );
            if ( index.contains( sig[ 0 ].atom ) ) error( "duplicate function " + sig[ 0 ].atom );
            index[ sig[ 0 ].atom ] = m.size();
            int n = sig.size() - 1;
            m.push_back( { sig[ 0 ].atom, n, n } );
        }

        /* pass 2: generate code */
        for ( const auto &form : program.items )
        {
            if ( is_define( form ) )
            {
                const auto &sig = form.items[ 1 ].items;
                function &f = m[ index[ sig[ 0 ].atom ] ];
                env params;
                for ( std::size_t k = 1; k < sig.size(); ++ k ) params[ sig[ k ].atom ] = k - 1;
                expr( f, form.items[ 2 ], params );
                emit( f, op::ret );
            }
            else
            {
                expr( m[ 0 ], form, {} );
                emit( m[ 0 ], op::pop );
            }
        }
        return m;
    }
};

/* The collector of ‹d2_copying›. */

word copy( space &from, space &to, word w )
{
    if ( !is_ptr( w ) ) return w;
    long a = untag( w );
    if ( from.mem[ a ] < 0 ) return tag_ptr( -1 - from.mem[ a ] );
    long n = from.mem[ a ], b = to.top;
    to.mem[ b ] = n;
    to.top += n + 1;
    from.mem[ a ] = -1 - b;
    for ( long k = 1; k <= n; ++ k )
        to.mem[ b + k ] = copy( from, to, from.mem[ a + k ] );
    return tag_ptr( b );
}

/* The virtual machine. The «roots» of the collector are exactly the
 * values the program can still use: the slots and operand stacks of
 * all active frames. Pay attention to ‹cons›: its two operands stay
 * «on the stack» while the allocation (and possibly a collection)
 * happens – had we popped them into C++ variables first, the
 * collector would not see them, and would neither keep the objects
 * alive nor update the pointers when the objects move. This is the
 * most common bug in hand-written runtime systems. */

struct result
{
    std::vector< long > output;
    int collections = 0;
    std::size_t max_live = 0;   /* the most words surviving a collection */
};

result execute( const module &m, std::size_t semispace )
{
    struct frame
    {
        int fn;
        std::size_t pc = 0;
        std::vector< word > slots, stack = {};
    };

    result res;
    space heap( semispace );
    std::vector< frame > frames{ { 0, 0, std::vector< word >( m[ 0 ].slots ) } };
    auto fail = []( const char *why ) { throw std::runtime_error( why ); };

    auto collect = [ & ]
    {
        space to( semispace );
        for ( auto &f : frames )
        {
            for ( auto &w : f.slots ) w = copy( heap, to, w );
            for ( auto &w : f.stack ) w = copy( heap, to, w );
        }
        heap = std::move( to );
        ++ res.collections;
        res.max_live = std::max( res.max_live, heap.top );
    };

    while ( true )
    {
        frame &f = frames.back();
        const auto &code = m[ f.fn ].code;
        if ( f.pc == code.size() ) return res;   /* only ‹main› has no ‹ret› at the end */

        const instr &i = code[ f.pc++ ];
        auto &st = f.stack;
        auto pop = [ & ] { word w = st.back(); st.pop_back(); return w; };
        auto pop_int = [ & ] { word w = pop(); if ( is_ptr( w ) ) fail( "not an integer" ); return untag( w ); };
        auto pop_pair = [ & ] { word w = pop(); if ( !is_ptr( w ) ) fail( "not a pair" ); return untag( w ); };

        switch ( i.o )
        {
            case op::push:  st.push_back( i.arg ); break;
            case op::load:  st.push_back( f.slots[ i.arg ] ); break;
            case op::store: f.slots[ i.arg ] = pop(); break;
            case op::pop:   pop(); break;
            case op::jump:  f.pc = i.arg; break;
            case op::jz:    if ( pop() == tag_int( 0 ) ) f.pc = i.arg; break;
            case op::print: { long v = pop_int(); res.output.push_back( v ); st.push_back( tag_int( v ) ); break; }
            case op::car:   st.push_back( heap.mem[ pop_pair() + 1 ] ); break;
            case op::cdr:   st.push_back( heap.mem[ pop_pair() + 2 ] ); break;
            case op::pairp: st.push_back( tag_int( is_ptr( pop() ) ) ); break;
            case op::eq:    { word b = pop(), a = pop(); st.push_back( tag_int( a == b ) ); break; }

            case op::cons:
            {
                auto a = heap.alloc( { st[ st.size() - 2 ], st.back() } );
                if ( !a )
                {
                    collect();
                    a = heap.alloc( { st[ st.size() - 2 ], st.back() } );   /* re-read: they moved */
                    if ( !a ) fail( "out of memory" );
                }
                st.resize( st.size() - 2 );
                st.push_back( tag_ptr( *a ) );
                break;
            }

            case op::call:
            {
                const function &g = m[ i.arg ];
                std::vector< word > slots( st.end() - g.params, st.end() );
                st.resize( st.size() - g.params );
                slots.resize( g.slots );
                frames.push_back( { int( i.arg ), 0, std::move( slots ) } );   /* invalidates ‹f› */
                break;
            }
            case op::ret:
            {
                word v = pop();
                frames.pop_back();
                frames.back().stack.push_back( v );
                break;
            }

            default:
            {
                long b = pop_int(), a = pop_int();
                st.push_back( tag_int( i.o == op::add ? a + b : i.o == op::sub ? a - b :
                                       i.o == op::mul ? a * b : a < b ) );
            }
        }
    }
}

result run( std::string_view source, std::size_t semispace )
{
    return execute( compiler().compile( read( source ) ), semispace );
}

int main()
{
    std::string lists = R"(
        (define (range a b) (if (< a b) (cons a (range (+ a 1) b)) nil))
        (define (sum l) (if (pair? l) (+ (car l) (sum (cdr l))) 0))
        (define (rev l acc) (if (pair? l) (rev (cdr l) (cons (car l) acc)) acc))
        (define (map-sq l) (if (pair? l) (let x (car l) (cons (* x x) (map-sq (cdr l)))) nil))
        (define (repeat n) (if (< 0 n) (begin (print (sum (range 0 1000))) (repeat (- n 1))) 0))
    )";

    auto r = run( "(" + lists + "(print (sum (range 1 11))) (print (car (rev (range 1 6) nil))))", 1000 );
    assert(( r.output == std::vector< long >{ 55, 5 } ));
    assert( r.collections == 0 );

    r = run( "(" + lists + "(print (sum (map-sq (range 1 4)))) (let l (range 0 3) (print (= l l))))", 1000 );
    assert(( r.output == std::vector< long >{ 14, 1 } ));

    /* each round builds a list of 1000 pairs (3000 words), sums it and
     * drops it: 10 rounds allocate 30000 words in a 4000-word semispace */
    r = run( "(" + lists + "(repeat 10))", 4000 );
    assert( r.output == std::vector< long >( 10, 499500 ) );
    assert( r.collections >= 6 );
    assert( r.max_live <= 3000 );

    /* the reversed list is built while the original is still live */
    r = run( "(" + lists + "(print (sum (rev (range 0 600) nil))))", 4000 );
    assert(( r.output == std::vector< long >{ 179700 } ));

    /* … but not with 1000 elements: 6000 live words do not fit */
    bool oom = false;
    try { run( "(" + lists + "(rev (range 0 1000) nil))", 4000 ); }
    catch ( const std::runtime_error & ) { oom = true; }
    assert( oom );

    /* static errors */
    for ( auto bad : { "((f 1))", "((define (f x) y))", "((define (f x) x) (f 1 2))", "((car 1 2))" } )
    {
        bool caught = false;
        try { run( bad, 100 ); } catch ( const std::invalid_argument & ) { caught = true; }
        assert( caught );
    }

    return 0;
}
