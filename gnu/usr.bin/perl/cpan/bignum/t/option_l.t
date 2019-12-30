#!perl

# test the "l", "lib", "try" and "only" options:

use strict;
use warnings;

use Test::More tests => 19;

use bignum;

# Catch warnings.

my @WARNINGS;
local $SIG{__WARN__} = sub {
    push @WARNINGS, $_[0];
};

my $rc;

$rc = eval { bignum->import( "l" => "foo" ) };
is($@, '',                     # shouldn't die
   qq|eval { bignum->import( "l" => "foo" ) }|);
is(scalar(@WARNINGS), 1, 'one warning');
like($WARNINGS[0], qr/fallback to Math::/, 'got fallback');

$rc = eval { bignum->import( "lib" => "foo" ) };
is($@, '',                     # ditto
   qq|eval { bignum->import( "lib" => "foo" ) }|);
is(scalar @WARNINGS, 2, 'two warnings');
like($WARNINGS[1], qr/fallback to Math::/, 'got fallback');

$rc = eval { bignum->import( "try" => "foo" ) };
is($@, '',                     # shouldn't die
   qq|eval { bignum->import( "try" => "foo" ) }|);

$rc = eval { bignum->import( "try" => "foo" ) };
is($@, '',                     # ditto
   qq|eval { bignum->import( "try" => "foo" ) }|);

$rc = eval { bignum->import( "foo" => "bar" ) };
like($@, qr/^Unknown option foo/i, 'died'); # should die

$rc = eval { bignum->import( "only" => "bar" ) };
like($@, qr/fallback disallowed/i, 'died'); # should die

# test that options are only lowercase (don't see a reason why allow UPPER)

foreach (qw/L LIB Lib T Trace TRACE V Version VERSION/) {
    $rc = eval { bignum->import( $_ => "bar" ) };
    like($@, qr/^Unknown option $_/i, 'died'); # should die
}
