use strict;
use warnings;
use Test::More tests => 30;

use Socket qw(:addrinfo AF_INET SOCK_STREAM IPPROTO_TCP unpack_sockaddr_in inet_aton);

my ( $err, @res );

( $err, @res ) = getaddrinfo( "127.0.0.1", "80", { socktype => SOCK_STREAM } );
cmp_ok( $err, "==", 0, '$err == 0 for host=127.0.0.1/service=80/socktype=STREAM' );
cmp_ok( $err, "eq", "", '$err eq "" for host=127.0.0.1/service=80/socktype=STREAM' );
is( scalar @res, 1,
    '@res has 1 result' );

is( $res[0]->{family}, AF_INET,
    '$res[0] family is AF_INET' );
is( $res[0]->{socktype}, SOCK_STREAM,
    '$res[0] socktype is SOCK_STREAM' );
ok( $res[0]->{protocol} == 0 || $res[0]->{protocol} == IPPROTO_TCP,
    '$res[0] protocol is 0 or IPPROTO_TCP' );
ok( defined $res[0]->{addr},
    '$res[0] addr is defined' );
if (length $res[0]->{addr}) {
    is_deeply( [ unpack_sockaddr_in $res[0]->{addr} ],
	       [ 80, inet_aton( "127.0.0.1" ) ],
	       '$res[0] addr is {"127.0.0.1", 80}' );
} else {
    fail( '$res[0] addr is empty: check $socksizetype' );
}

# Check actual IV integers work just as well as PV strings
( $err, @res ) = getaddrinfo( "127.0.0.1", 80, { socktype => SOCK_STREAM } );
cmp_ok( $err, "==", 0, '$err == 0 for host=127.0.0.1/service=80/socktype=STREAM' );
is_deeply( [ unpack_sockaddr_in $res[0]->{addr} ],
           [ 80, inet_aton( "127.0.0.1" ) ],
           '$res[0] addr is {"127.0.0.1", 80}' );

( $err, @res ) = getaddrinfo( "127.0.0.1", "" );
cmp_ok( $err, "==", 0, '$err == 0 for host=127.0.0.1' );
# Might get more than one; e.g. different socktypes
ok( scalar @res > 0, '@res has results' );

( $err, @res ) = getaddrinfo( "127.0.0.1", undef );
cmp_ok( $err, "==", 0, '$err == 0 for host=127.0.0.1/service=undef' );

# Test GETMAGIC
{
    "127.0.0.1" =~ /(.+)/;
    ( $err, @res ) = getaddrinfo($1, undef);
    cmp_ok( $err, "==", 0, '$err == 0 for host=$1' );
    ok( scalar @res > 0, '@res has results' );
    is( (unpack_sockaddr_in $res[0]->{addr})[1],
	inet_aton( "127.0.0.1" ),
	'$res[0] addr is {"127.0.0.1", ??}' );
}

( $err, @res ) = getaddrinfo( "", "80", { family => AF_INET, socktype => SOCK_STREAM, protocol => IPPROTO_TCP } );
cmp_ok( $err, "==", 0, '$err == 0 for service=80/family=AF_INET/socktype=STREAM/protocol=IPPROTO_TCP' );
is( scalar @res, 1, '@res has 1 result' );

# Just pick the first one
is( $res[0]->{family}, AF_INET,
    '$res[0] family is AF_INET' );
is( $res[0]->{socktype}, SOCK_STREAM,
    '$res[0] socktype is SOCK_STREAM' );
ok( $res[0]->{protocol} == 0 || $res[0]->{protocol} == IPPROTO_TCP,
    '$res[0] protocol is 0 or IPPROTO_TCP' );

# Now some tests of a few well-known internet hosts
my $goodhost = "cpan.perl.org";

SKIP: {
    skip "Resolver has no answer for $goodhost", 2 unless gethostbyname( $goodhost );

    ( $err, @res ) = getaddrinfo( "cpan.perl.org", "ftp", { socktype => SOCK_STREAM } );
    cmp_ok( $err, "==", 0, '$err == 0 for host=cpan.perl.org/service=ftp/socktype=STREAM' );
    # Might get more than one; e.g. different families
    ok( scalar @res > 0, '@res has results' );
}

# Now something I hope doesn't exist - we put it in a known-missing TLD
my $missinghost = "TbK4jM2M0OS.lm57DWIyu4i";

# Some CPAN testing machines seem to have wildcard DNS servers that reply to
# any request. We'd better check for them

SKIP: {
    skip "Resolver has an answer for $missinghost", 1 if gethostbyname( $missinghost );

    # Some OSes return $err == 0 but no results
    ( $err, @res ) = getaddrinfo( $missinghost, "ftp", { socktype => SOCK_STREAM } );
    ok( $err != 0 || ( $err == 0 && @res == 0 ),
	'$err != 0 or @res == 0 for host=TbK4jM2M0OS.lm57DWIyu4i/service=ftp/socktype=SOCK_STREAM' );
    if( @res ) {
	# Diagnostic that might help
	while( my $r = shift @res ) {
	    diag( "family=$r->{family} socktype=$r->{socktype} protocol=$r->{protocol} addr=[" . length( $r->{addr} ) . " bytes]" );
	    diag( "  addr=" . join( ", ", map { sprintf '0x%02x', ord $_ } split m//, $r->{addr} ) );
	}
    }
}

# Now check that names with AI_NUMERICHOST fail

SKIP: {
    skip "Resolver has no answer for $goodhost", 1 unless gethostbyname( $goodhost );

    ( $err, @res ) = getaddrinfo( $goodhost, "ftp", { flags => AI_NUMERICHOST, socktype => SOCK_STREAM } );
    ok( $err != 0, "\$err != 0 for host=$goodhost/service=ftp/flags=AI_NUMERICHOST/socktype=SOCK_STREAM" );
}

# Some sanity checking on the hints hash
ok( defined eval { getaddrinfo( "127.0.0.1", "80", undef ); 1 },
    'getaddrinfo() with undef hints works' );
ok( !defined eval { getaddrinfo( "127.0.0.1", "80", "hints" ); 1 },
    'getaddrinfo() with string hints dies' );
ok( !defined eval { getaddrinfo( "127.0.0.1", "80", [] ); 1 },
    'getaddrinfo() with ARRAY hints dies' );

# Ensure it doesn't segfault if args are missing

( $err, @res ) = getaddrinfo();
ok( defined $err, '$err defined for getaddrinfo()' );

( $err, @res ) = getaddrinfo( "127.0.0.1" );
ok( defined $err, '$err defined for getaddrinfo("127.0.0.1")' );
