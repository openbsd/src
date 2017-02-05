#!perl

use strict;
use warnings;

use Test::More tests => 2409            # tests in require'd file
                        + 5;            # tests in this file

use Math::BigInt lib => 'Calc';
use Math::BigFloat;

our $CLASS = "Math::BigFloat";
our $CALC  = "Math::BigInt::Calc";      # backend

is($CLASS->config()->{class}, $CLASS, "$CLASS->config()->{class}");
is($CLASS->config()->{with},  $CALC,  "$CLASS->config()->{with}");

# bug #17447: Can't call method Math::BigFloat->bsub, not a valid method
my $c = Math::BigFloat->new('123.3');
is($c->bsub(123), '0.3',
   qq|\$c = Math::BigFloat -> new("123.3"); \$y = \$c -> bsub("123")|);

# Bug until Math::BigInt v1.86, the scale wasn't treated as a scalar:
$c = Math::BigFloat->new('0.008');
my $d = Math::BigFloat->new(3);
my $e = $c->bdiv(Math::BigFloat->new(3), $d);

is($e, '0.00267', '0.008 / 3 = 0.0027');

SKIP: {
    skip("skipping test which is not for this backend", 1)
      unless $CALC eq 'Math::BigInt::Calc';
    is(ref($e->{_e}->[0]), '', '$e->{_e}->[0] is a scalar');
}

require 't/bigfltpm.inc';	# all tests here for sharing
