#!/usr/bin/perl -w

# Test whether $Math::BigInt::upgrade breaks our neck

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 5;
  }

use Math::BigInt upgrade => 'Math::BigRat';
use Math::BigRat;

my $rat = 'Math::BigRat';
my ($x,$y,$z);

##############################################################################
# bceil/bfloor

$x = $rat->new('49/4'); is ($x->bfloor(),'12', 'floor(49/4)');
$x = $rat->new('49/4'); is ($x->bceil(),'13', 'ceil(49/4)');

##############################################################################
# bsqrt

$x = $rat->new('144'); is ($x->bsqrt(),'12', 'bsqrt(144)');
$x = $rat->new('144/16'); is ($x->bsqrt(),'3', 'bsqrt(144/16)');
$x = $rat->new('1/3'); is ($x->bsqrt(),
 '1000000000000000000000000000000000000000/1732050807568877293527446341505872366943',
 'bsqrt(1/3)');

# all tests successfull

1;

