#include <cassert>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

/* Not every language is about functions and statements. «Datalog» is
 * a logic programming language (a subset of Prolog) used for database
 * queries, static program analysis (many pointer analyses are written
 * in it) and access control. A program consists of «facts» and
 * «rules», from which new facts are derived until nothing new can be
 * derived:
 *
 *     parent(alice, bob).
 *     parent(bob, carol).
 *     ancestor(X, Y) :- parent(X, Y).
 *     ancestor(X, Z) :- parent(X, Y), ancestor(Y, Z).
 *     ?- ancestor(alice, W).
 *
 * The last line is a «query», whose answers are ‹W = bob› and
 * ‹W = carol›. Write an interpreter for Datalog with stratified negation.
 *
 * ── Syntax ─────────────────────────────────────────────────────────
 *
 *     program = { clause | query } ;
 *     clause  = atom, [ ':-', literal, { ',', literal } ], '.' ;
 *     query   = '?-', literal, { ',', literal }, '.' ;
 *     literal = atom | 'not', atom | term, cmp, term ;
 *     cmp     = '=' | '!=' | '<' | '<=' ;
 *     atom    = name, [ '(', term, { ',', term }, ')' ] ;
 *     term    = name | integer | variable ;
 *
 * A ‹name› (of a predicate or a constant) starts with a lowercase
 * letter, a ‹variable› with an uppercase letter or ‹_›; both continue
 * with letters, digits and ‹_›. Integers are sequences of digits
 * (constants, too). ‹not› is a keyword. Whitespace separates tokens,
 * and ‹%› starts a comment extending to the end of the line. An atom
 * without parentheses has no arguments (a «propositional» predicate,
 * like ‹raining›).
 *
 * ── Static checks ──────────────────────────────────────────────────
 *
 * Return ‹std::nullopt› if the program is syntactically invalid, or if:
 *
 *  • a predicate is used with different numbers of arguments,
 *  • a clause or query is not «safe»: every variable which appears in
 *    the head of a clause, in a negated atom or in a comparison must
 *    also appear in a positive atom of the same body (so a fact must be
 *    ground); the variable ‹_› is «anonymous» – each occurrence is a
 *    different variable – and it may only appear in positive atoms,
 *  • the program is not «stratifiable»: a predicate depends on itself
 *    through a negation (‹p :- not q.› and ‹q :- p.›).
 *
 * ── Semantics ──────────────────────────────────────────────────────
 *
 * A predicate is «defined» by the clauses with it in the head.
 * Predicates are evaluated in «strata»: a predicate which is used
 * under ‹not› by the clauses of ‹p› must be completely computed before
 * ‹p›. Within a stratum, rules are applied until no new facts appear
 * (the least fixpoint). A positive atom in a body matches the facts
 * of its predicate (binding variables consistently); ‹not a› succeeds
 * if ‹a› (with all its variables bound) is not a fact; ‹=› and ‹!=›
 * compare two constants for equality, ‹<› and ‹<=› compare two integer
 * constants (and are false if either is not an integer). A predicate
 * that is never defined has no facts.
 *
 * Use «semi-naive» evaluation, or at least don't re-derive everything
 * from scratch in a quadratic number of passes – the tests include a
 * transitive closure with ten thousand facts.
 *
 * ── Output ─────────────────────────────────────────────────────────
 *
 * The result is a list of lines: for each query, in order, its
 * answers. If the query has (non-anonymous) variables, each answer is
 * a line ‹X = a, Y = b› giving the values of the variables in the order
 * of their first appearance in the query; the lines of one query are
 * distinct and sorted (as strings); if there are no answers, the
 * single line is ‹false›. A query without variables gives the line
 * ‹true› or ‹false›. */

// std::optional< std::vector< std::string > > run( std::string_view program );

struct datalog_error {};

/* ─── syntax ─────────────────────────────────────────────────────── */

struct term { bool var; std::string name; };
struct atom_t { std::string pred; std::vector< term > args; };

struct literal
{
    enum kind { pos, neg, cmp } k;
    atom_t a = {};                 /* for pos / neg */
    term l = {}, r = {};           /* for cmp */
    std::string op = {};
};

struct rule { atom_t head; std::vector< literal > body; };
struct query { std::vector< literal > body; };

struct dl_parser
{
    std::vector< std::string > toks;
    std::size_t pos = 0;

    static std::vector< std::string > lex( std::string_view s )
    {
        auto word = []( char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) ||
                                          ( c >= '0' && c <= '9' ) || c == '_'; };
        std::vector< std::string > out;
        for ( std::size_t i = 0; i < s.size(); )
        {
            char c = s[ i ];
            if ( c == ' ' || c == '\n' || c == '\t' ) { ++ i; continue; }
            if ( c == '%' ) { while ( i < s.size() && s[ i ] != '\n' ) ++ i; continue; }
            std::size_t start = i;
            if ( word( c ) )
                while ( i < s.size() && word( s[ i ] ) ) ++ i;
            else if ( s.substr( i, 2 ) == ":-" || s.substr( i, 2 ) == "?-" ||
                      s.substr( i, 2 ) == "!=" || s.substr( i, 2 ) == "<=" )
                i += 2;
            else if ( std::string_view( "(),.=<" ).find( c ) != std::string_view::npos )
                ++ i;
            else
                throw datalog_error();
            out.emplace_back( s.substr( start, i - start ) );
        }
        return out;
    }

    const std::string &peek() const
    {
        static const std::string end;
        return pos < toks.size() ? toks[ pos ] : end;
    }

    bool accept( std::string_view t ) { if ( peek() != t ) return false; ++ pos; return true; }
    void expect( std::string_view t ) { if ( !accept( t ) ) throw datalog_error(); }

    static bool is_name( const std::string &t ) { return !t.empty() && t[ 0 ] >= 'a' && t[ 0 ] <= 'z' && t != "not"; }
    static bool is_var( const std::string &t ) { return !t.empty() && ( ( t[ 0 ] >= 'A' && t[ 0 ] <= 'Z' ) || t[ 0 ] == '_' ); }
    static bool is_int( const std::string &t ) { return !t.empty() && t.find_first_not_of( "0123456789" ) == t.npos; }

    term parse_term()
    {
        std::string t = peek();
        if ( !is_name( t ) && !is_var( t ) && !is_int( t ) ) throw datalog_error();
        ++ pos;
        return { is_var( t ), t };
    }

    atom_t parse_atom()
    {
        if ( !is_name( peek() ) ) throw datalog_error();
        atom_t a{ toks[ pos++ ], {} };
        if ( accept( "(" ) )
        {
            do a.args.push_back( parse_term() ); while ( accept( "," ) );
            expect( ")" );
        }
        return a;
    }

    literal parse_literal()
    {
        if ( accept( "not" ) )
            return { literal::neg, parse_atom() };

        /* an atom starts with a name followed by ‹(›, or a name which is
         * not followed by a comparison */
        std::string next = pos + 1 < toks.size() ? toks[ pos + 1 ] : "";
        bool comparison = next == "=" || next == "!=" || next == "<" || next == "<=";
        if ( is_name( peek() ) && !comparison )
            return { literal::pos, parse_atom() };

        literal l{ literal::cmp };
        l.l = parse_term();
        l.op = peek();
        if ( l.op != "=" && l.op != "!=" && l.op != "<" && l.op != "<=" ) throw datalog_error();
        ++ pos;
        l.r = parse_term();
        return l;
    }

    std::vector< literal > body()
    {
        std::vector< literal > out;
        do out.push_back( parse_literal() ); while ( accept( "," ) );
        return out;
    }
};

/* ─── evaluation ─────────────────────────────────────────────────── */

using tuple = std::vector< std::string >;
using relation = std::set< tuple >;
using binding = std::map< std::string, std::string >;

struct engine
{
    std::vector< rule > rules;
    std::vector< query > queries;
    std::map< std::string, std::size_t > arity;
    std::map< std::string, relation > facts;

    void use( const atom_t &a )
    {
        auto [ it, fresh ] = arity.emplace( a.pred, a.args.size() );
        if ( !fresh && it->second != a.args.size() ) throw datalog_error();
    }

    static void check_safe( const std::vector< literal > &body, const std::vector< term > &extra )
    {
        std::set< std::string > bound;
        for ( const auto &l : body )
            if ( l.k == literal::pos )
                for ( const auto &t : l.a.args )
                    if ( t.var && t.name != "_" ) bound.insert( t.name );

        auto need = [ & ]( const term &t )
        {
            if ( t.var && ( t.name == "_" || !bound.contains( t.name ) ) ) throw datalog_error();
        };

        for ( const auto &t : extra ) need( t );
        for ( const auto &l : body )
            if ( l.k == literal::neg )
                for ( const auto &t : l.a.args ) need( t );
            else if ( l.k == literal::cmp )
                need( l.l ), need( l.r );
    }

    /* Match an atom against a tuple, extending the binding. */
    static bool match( const atom_t &a, const tuple &t, binding &b )
    {
        for ( std::size_t i = 0; i < a.args.size(); ++ i )
        {
            const term &x = a.args[ i ];
            if ( !x.var ) { if ( x.name != t[ i ] ) return false; continue; }
            if ( x.name == "_" ) continue;
            auto [ it, fresh ] = b.emplace( x.name, t[ i ] );
            if ( !fresh && it->second != t[ i ] ) return false;
        }
        return true;
    }

    static std::string value( const term &t, const binding &b ) { return t.var ? b.at( t.name ) : t.name; }

    static bool compare( const literal &l, const binding &b )
    {
        std::string x = value( l.l, b ), y = value( l.r, b );
        if ( l.op == "=" ) return x == y;
        if ( l.op == "!=" ) return x != y;
        if ( !dl_parser::is_int( x ) || !dl_parser::is_int( y ) ) return false;
        long a = std::stol( x ), c = std::stol( y );
        return l.op == "<" ? a < c : a <= c;
    }

    /* Enumerate the bindings satisfying ‹body›. Positive literals are
     * matched against ‹facts›, except the one at position ‹delta_at›,
     * which is matched against ‹delta› (semi-naive evaluation). Negated
     * and comparison literals are checked once all positive literals are
     * done (safety guarantees their variables are bound by then). */
    void solve( const std::vector< literal > &body, std::size_t i, binding &b,
                std::size_t delta_at, const relation *delta, const auto &emit )
    {
        if ( i == body.size() )
        {
            for ( const auto &l : body )
                if ( l.k == literal::cmp && !compare( l, b ) ) return;
                else if ( l.k == literal::neg )
                {
                    tuple t;
                    for ( const auto &x : l.a.args ) t.push_back( value( x, b ) );
                    if ( facts[ l.a.pred ].contains( t ) ) return;
                }
            emit( b );
            return;
        }

        const literal &l = body[ i ];
        if ( l.k != literal::pos )
            return solve( body, i + 1, b, delta_at, delta, emit );

        const relation &r = i == delta_at ? *delta : facts[ l.a.pred ];
        for ( const auto &t : r )
        {
            binding nb = b;
            if ( match( l.a, t, nb ) )
                solve( body, i + 1, nb, delta_at, delta, emit );
        }
    }

    tuple instantiate( const atom_t &a, const binding &b )
    {
        tuple t;
        for ( const auto &x : a.args ) t.push_back( value( x, b ) );
        return t;
    }

    void evaluate()
    {
        /* strata: stratum(p) ≥ stratum(q) for positive dependencies,
         * > for negative ones; a cycle through negation shows up as a
         * stratum which keeps growing */
        std::map< std::string, std::size_t > stratum;
        for ( const auto &[ p, _ ] : arity ) stratum[ p ] = 0;
        for ( bool changed = true; changed; )
        {
            changed = false;
            for ( const auto &r : rules )
                for ( const auto &l : r.body )
                    if ( l.k != literal::cmp )
                    {
                        std::size_t need = stratum[ l.a.pred ] + ( l.k == literal::neg );
                        if ( stratum[ r.head.pred ] < need )
                        {
                            stratum[ r.head.pred ] = need;
                            changed = true;
                            if ( need > arity.size() ) throw datalog_error();
                        }
                    }
        }

        std::size_t top = 0;
        for ( const auto &[ _, s ] : stratum ) top = std::max( top, s );

        for ( std::size_t s = 0; s <= top; ++ s )
        {
            std::vector< const rule * > here;
            for ( const auto &r : rules )
                if ( stratum[ r.head.pred ] == s ) here.push_back( &r );

            /* first round: all rules, naively */
            std::map< std::string, relation > delta;
            for ( auto r : here )
            {
                binding b;
                solve( r->body, 0, b, std::size_t( -1 ), nullptr, [ & ]( const binding &x )
                {
                    tuple t = instantiate( r->head, x );
                    if ( !facts[ r->head.pred ].contains( t ) ) delta[ r->head.pred ].insert( t );
                } );
            }

            while ( true )
            {
                bool any = false;
                for ( auto &[ p, d ] : delta )
                    for ( const auto &t : d )
                        any = facts[ p ].insert( t ).second || any;
                if ( !any ) break;

                /* next round: each rule once per positive body literal
                 * over a predicate of this stratum with a non-empty
                 * delta, using the delta for that literal */
                std::map< std::string, relation > next;
                for ( auto r : here )
                    for ( std::size_t i = 0; i < r->body.size(); ++ i )
                    {
                        const literal &l = r->body[ i ];
                        if ( l.k != literal::pos || !delta.contains( l.a.pred ) || delta[ l.a.pred ].empty() )
                            continue;
                        binding b;
                        solve( r->body, 0, b, i, &delta[ l.a.pred ], [ & ]( const binding &x )
                        {
                            tuple t = instantiate( r->head, x );
                            if ( !facts[ r->head.pred ].contains( t ) ) next[ r->head.pred ].insert( t );
                        } );
                    }
                delta = std::move( next );
            }
        }
    }

    std::vector< std::string > answer( const query &q )
    {
        std::vector< std::string > vars;
        for ( const auto &l : q.body )
        {
            std::vector< term > ts = l.k == literal::cmp ? std::vector< term >{ l.l, l.r } : l.a.args;
            for ( const auto &t : ts )
                if ( t.var && t.name != "_" && std::find( vars.begin(), vars.end(), t.name ) == vars.end() )
                    vars.push_back( t.name );
        }

        std::set< std::string > lines;
        bool found = false;
        binding b;
        solve( q.body, 0, b, std::size_t( -1 ), nullptr, [ & ]( const binding &x )
        {
            found = true;
            std::string line;
            for ( const auto &v : vars )
                line += ( line.empty() ? "" : ", " ) + v + " = " + x.at( v );
            lines.insert( line );
        } );

        if ( vars.empty() ) return { found ? "true" : "false" };
        if ( lines.empty() ) return { "false" };
        return { lines.begin(), lines.end() };
    }
};

std::optional< std::vector< std::string > > run( std::string_view program )
{
    try
    {
        dl_parser p{ dl_parser::lex( program ) };
        engine e;

        while ( p.pos < p.toks.size() )
        {
            if ( p.accept( "?-" ) )
            {
                query q{ p.body() };
                p.expect( "." );
                e.queries.push_back( q );
                continue;
            }
            rule r{ p.parse_atom(), {} };
            if ( p.accept( ":-" ) )
                r.body = p.body();
            p.expect( "." );
            e.rules.push_back( r );
        }

        for ( const auto &r : e.rules )
        {
            e.use( r.head );
            for ( const auto &l : r.body ) if ( l.k != literal::cmp ) e.use( l.a );
            for ( const auto &t : r.head.args )
                if ( t.var && t.name == "_" ) throw datalog_error();
            engine::check_safe( r.body, r.head.args );
        }
        for ( const auto &q : e.queries )
        {
            for ( const auto &l : q.body ) if ( l.k != literal::cmp ) e.use( l.a );
            engine::check_safe( q.body, {} );
        }

        e.evaluate();

        std::vector< std::string > out;
        for ( const auto &q : e.queries )
            for ( auto &line : e.answer( q ) )
                out.push_back( line );
        return out;
    }
    catch ( const datalog_error & ) { return std::nullopt; }
}

int main()
{
    using v = std::vector< std::string >;

    std::string family =
        "parent(alice, bob). parent(bob, carol). parent(bob, dave). parent(carol, eve).\n"
        "male(bob). male(dave).\n"
        "ancestor(X, Y) :- parent(X, Y).\n"
        "ancestor(X, Z) :- parent(X, Y), ancestor(Y, Z).  % recursion\n"
        "sibling(X, Y) :- parent(P, X), parent(P, Y), X != Y.\n"
        "has_child(X) :- parent(X, _).\n"
        "childless_male(X) :- male(X), not has_child(X).\n";

    assert(( run( family + "?- ancestor(alice, W)." ) == v{ "W = bob", "W = carol", "W = dave", "W = eve" } ));
    assert(( run( family + "?- sibling(A, B)." ) == v{ "A = carol, B = dave", "A = dave, B = carol" } ));
    assert(( run( family + "?- childless_male(X)." ) == v{ "X = dave" } ));
    assert(( run( family + "?- ancestor(bob, eve). ?- ancestor(eve, bob)." ) == v{ "true", "false" } ));
    assert(( run( family + "?- parent(X, Y), ancestor(Y, eve)." ) == v{ "X = alice, Y = bob", "X = bob, Y = carol" } ));
    assert(( run( family + "?- ancestor(nobody, X)." ) == v{ "false" } ));
    assert(( run( family + "?- parent(_, X), not has_child(X)." ) == v{ "X = dave", "X = eve" } ));

    assert(( run( "raining. wet :- raining. dry :- not wet. ?- wet. ?- dry." ) == v{ "true", "false" } ));
    assert(( run( "n(1). n(2). n(3). n(10). small(X) :- n(X), X < 3. ?- small(X). ?- n(X), X <= 2, X = 2." ) ==
             v{ "X = 1", "X = 2", "X = 2" } ));
    assert(( run( "q(a). ?- q(X), X < 5." ) == v{ "false" } ));
    assert(( run( "" ) == v{} ));

    /* a long chain: path has n·(n - 1)/2 facts */
    std::string chain;
    const int n = 150;
    for ( int i = 0; i + 1 < n; ++ i )
        chain += "edge(n" + std::to_string( i ) + ", n" + std::to_string( i + 1 ) + ").\n";
    chain += "path(X, Y) :- edge(X, Y).\n"
             "path(X, Z) :- path(X, Y), edge(Y, Z).\n"
             "?- path(n0, X).\n"
             "?- path(X, n0).\n"
             "?- path(n3, n140).\n";
    auto r = run( chain );
    assert( r && r->size() == std::size_t( n - 1 + 1 + 1 ) );
    assert( ( *r )[ n - 1 ] == "false" && ( *r )[ n ] == "true" );

    assert( !run( "p(X)." ) );                                  /* unsafe fact */
    assert( !run( "p(X) :- q(Y)." ) );                          /* unsafe head */
    assert( !run( "p(X) :- q(X), not r(Y)." ) );                /* unsafe negation */
    assert( !run( "p(X) :- X = 1." ) );                         /* unsafe comparison */
    assert( !run( "p(X) :- q(X), not r(_)." ) );                /* anonymous under not */
    assert( !run( "p(a). p(a, b)." ) );                         /* arity */
    assert( !run( "p :- not q. q :- p." ) );                    /* not stratifiable */
    assert( !run( "p(a)" ) );                                   /* missing ‹.› */
    assert( !run( "P(a)." ) );
    assert( !run( "?- q(X), not r(Y)." ) );

    return 0;
}
