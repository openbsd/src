#!/usr/bin/perl -w

###############################################################################

use Test;
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
ok ($@,'');						# shouldn't die
$rc = eval ('bignum->import( "lib" => "foo" );');
ok ($@,'');						# ditto

$rc = eval ('bignum->import( "foo" => "bar" );');
ok ($@ =~ /^Unknown option foo/i,1);			# should die

# test that options are only lowercase (don't see a reason why allow UPPER)

foreach (qw/L LIB Lib T Trace TRACE V Version VERSION/)
  {
  $rc = eval ('bignum->import( "$_" => "bar" );');
  ok ($@ =~ /^Unknown option $_/i,1);			# should die
  }

