#!/usr/bin/perl -w

# test for bug #34584: hang in exp(1/2)

use strict;
use Test::More;

BEGIN 
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 1;
  }

use Math::BigRat;

my $result = Math::BigRat->new('1/2')->bexp();

is ("$result", "9535900335500879457687887524133067574481/5783815921445270815783609372070483523265",
    "exp(1/2) worked");

##############################################################################
# done

1;

