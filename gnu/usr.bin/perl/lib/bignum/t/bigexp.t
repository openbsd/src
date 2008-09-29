#!/usr/bin/perl -w

###############################################################################
# test for bug #18025: bignum/bigrat can lead to a number that is both 1 and 0

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 4;
  }

use bignum;

my $lnev = -7 / (10**17);
my $ev=exp($lnev);

is( sprintf('%0.5f',$ev) , '1.00000', '($ev) is approx. 1' );
is( sprintf('%0.5f',1-$ev) , '0.00000', '(1-$ev) is approx. 0' );
is( sprintf('%0.5f',1-"$ev") , '0.00000', '(1-"$ev") is approx. 0' );

cmp_ok( $ev, '!=', 0, '$ev should not equal 0');
