#!/usr/bin/perl -w

use strict;
use Test::More tests => 15;

use Math::BigInt upgrade => 'Math::BigFloat';
use Math::BigFloat downgrade => 'Math::BigInt', upgrade => 'Math::BigInt';

use vars qw ($scale $class $try $x $y $f @args $ans $ans1 $ans1_str $setup
             $ECL $CL);
$class = "Math::BigInt";
$CL = "Math::BigInt::Calc";
$ECL = "Math::BigFloat";

# simplistic test for now 
is (Math::BigFloat->downgrade(),'Math::BigInt');
is (Math::BigFloat->upgrade(),'Math::BigInt');

# these downgrade
is (ref(Math::BigFloat->new('inf')),'Math::BigInt');
is (ref(Math::BigFloat->new('-inf')),'Math::BigInt');
is (ref(Math::BigFloat->new('NaN')),'Math::BigInt');
is (ref(Math::BigFloat->new('0')),'Math::BigInt');
is (ref(Math::BigFloat->new('1')),'Math::BigInt');
is (ref(Math::BigFloat->new('10')),'Math::BigInt');
is (ref(Math::BigFloat->new('-10')),'Math::BigInt');
is (ref(Math::BigFloat->new('-10.0E1')),'Math::BigInt');

# bug until v1.67:
is (Math::BigFloat->new('0.2E0'), '0.2');
is (Math::BigFloat->new('0.2E1'), '2');
# until v1.67 resulted in 200:
is (Math::BigFloat->new('0.2E2'), '20');

# disable, otherwise it screws calculations
Math::BigFloat->upgrade(undef);
is (Math::BigFloat->upgrade()||'','');

Math::BigFloat->div_scale(20); 				# make it a bit faster
my $x = Math::BigFloat->new(2);				# downgrades
# the following test upgrade for bsqrt() and also makes new() NOT downgrade
# for the bpow() side
is (Math::BigFloat->bpow('2','0.5'),$x->bsqrt());

#require 'upgrade.inc';	# all tests here for sharing
