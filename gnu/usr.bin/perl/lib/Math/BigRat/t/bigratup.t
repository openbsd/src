#!/usr/bin/perl -w

# Test whether $Math::BigInt::upgrade is breaks out neck

use Test;
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

$x = $rat->new('49/4'); ok ($x->bfloor(),'12');
$x = $rat->new('49/4'); ok ($x->bceil(),'13');

##############################################################################
# bsqrt

$x = $rat->new('144'); ok ($x->bsqrt(),'12');
$x = $rat->new('144/16'); ok ($x->bsqrt(),'3');
$x = $rat->new('1/3'); ok ($x->bsqrt(),
 '1000000000000000000000000000000000000000/1732050807568877293527446341505872366943');




