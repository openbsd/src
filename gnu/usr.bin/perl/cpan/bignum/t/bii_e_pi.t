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
  plan tests => 5;
  }

use bigint qw/e PI bpi bexp/;

is (e, "2", 'e');
is (PI, "3", 'PI');

is (bexp(1,10), "2", 'e');
is (bexp(3,10), "20", 'e');
is (bpi(10), "3", 'PI');
