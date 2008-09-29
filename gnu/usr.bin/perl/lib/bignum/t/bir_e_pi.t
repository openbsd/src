#!/usr/bin/perl -w

###############################################################################
# test for e() and PI() exports

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 4;
  }

use bigrat qw/e PI bexp bpi/;

is (e, "2.718281828459045235360287471352662497757", 'e');
is (PI, "3.141592653589793238462643383279502884197", 'PI');

# these tests should actually produce big rationals, but this is not yet
# implemented:
is (bexp(1,10), "2.718281828", 'e');
is (bpi(10), "3.141592654", 'PI');
