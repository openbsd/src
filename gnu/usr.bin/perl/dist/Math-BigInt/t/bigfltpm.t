#!/usr/bin/perl -w

use strict;
use Test::More tests => 2338
    + 5;		# own tests


use Math::BigInt lib => 'Calc';
use Math::BigFloat;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigFloat";
$CL = "Math::BigInt::Calc";

is ($class->config()->{class},$class);
is ($class->config()->{with}, $CL);

# bug #17447: Can't call method Math::BigFloat->bsub, not a valid method
my $c = Math::BigFloat->new( '123.3' );
is ($c->fsub(123), '0.3'); # calling fsub on a BigFloat works

# Bug until BigInt v1.86, the scale wasn't treated as a scalar:
$c = Math::BigFloat->new('0.008'); my $d = Math::BigFloat->new(3);
my $e = $c->bdiv(Math::BigFloat->new(3),$d);

is ($e,'0.00267'); # '0.008 / 3 => 0.0027');
is (ref($e->{_e}->[0]), ''); # 'Not a BigInt');

require 't/bigfltpm.inc';	# all tests here for sharing
