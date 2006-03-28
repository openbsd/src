#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 12;
  }

use bignum;

my $rc = eval ('bignum->import( "l" => "foo" );');
is ($@,'');						# shouldn't die
$rc = eval ('bignum->import( "lib" => "foo" );');
is ($@,'');						# ditto

$rc = eval ('bignum->import( "foo" => "bar" );');
like ($@, qr/^Unknown option foo/i, 'died');			# should die

# test that options are only lowercase (don't see a reason why allow UPPER)

foreach (qw/L LIB Lib T Trace TRACE V Version VERSION/)
  {
  $rc = eval ('bignum->import( "$_" => "bar" );');
  like ($@, qr/^Unknown option $_/i, 'died');			# should die
  }

