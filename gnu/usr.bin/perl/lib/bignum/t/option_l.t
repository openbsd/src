#!/usr/bin/perl -w

# test the "l", "lib", "try" and "only" options:

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 19;
  }

use bignum;

my @W;
{
# catch warnings:
require Carp;
no warnings 'redefine';
*Carp::carp = sub { push @W, $_[0]; };
}

my $rc = eval ('bignum->import( "l" => "foo" );');
is ($@,'');						# shouldn't die
is (scalar @W, 1, 'one warning');
like ($W[0], qr/fallback to Math::/, 'got fallback');

$rc = eval ('bignum->import( "lib" => "foo" );');
is ($@,'');						# ditto
is (scalar @W, 2, 'two warnings');
like ($W[1], qr/fallback to Math::/, 'got fallback');

$rc = eval ('bignum->import( "try" => "foo" );');
is ($@,'');						# shouldn't die
$rc = eval ('bignum->import( "try" => "foo" );');
is ($@,'');						# ditto

$rc = eval ('bignum->import( "foo" => "bar" );');
like ($@, qr/^Unknown option foo/i, 'died');			# should die

$rc = eval ('bignum->import( "only" => "bar" );');
like ($@, qr/fallback disallowed/i, 'died');			# should die

# test that options are only lowercase (don't see a reason why allow UPPER)

foreach (qw/L LIB Lib T Trace TRACE V Version VERSION/)
  {
  $rc = eval ('bignum->import( "$_" => "bar" );');
  like ($@, qr/^Unknown option $_/i, 'died');			# should die
  }

