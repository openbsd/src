#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  unshift @INC, '../lib'; # for running manually
  my $location = $0; $location =~ s/downgrade.t//;
  unshift @INC, $location; # to locate the testing files
  chdir 't' if -d 't';
  plan tests => 15;
  }

use Math::BigInt upgrade => 'Math::BigFloat';
use Math::BigFloat downgrade => 'Math::BigInt', upgrade => 'Math::BigInt';

use vars qw ($scale $class $try $x $y $f @args $ans $ans1 $ans1_str $setup
             $ECL $CL);
$class = "Math::BigInt";
$CL = "Math::BigInt::Calc";
$ECL = "Math::BigFloat";

# simplistic test for now 
ok (Math::BigFloat->downgrade(),'Math::BigInt');
ok (Math::BigFloat->upgrade(),'Math::BigInt');

# these downgrade
ok (ref(Math::BigFloat->new('inf')),'Math::BigInt');
ok (ref(Math::BigFloat->new('-inf')),'Math::BigInt');
ok (ref(Math::BigFloat->new('NaN')),'Math::BigInt');
ok (ref(Math::BigFloat->new('0')),'Math::BigInt');
ok (ref(Math::BigFloat->new('1')),'Math::BigInt');
ok (ref(Math::BigFloat->new('10')),'Math::BigInt');
ok (ref(Math::BigFloat->new('-10')),'Math::BigInt');
ok (ref(Math::BigFloat->new('-10.0E1')),'Math::BigInt');

# bug until v1.67:
ok (Math::BigFloat->new('0.2E0'), '0.2');
ok (Math::BigFloat->new('0.2E1'), '2');
# until v1.67 resulted in 200:
ok (Math::BigFloat->new('0.2E2'), '20');

# disable, otherwise it screws calculations
Math::BigFloat->upgrade(undef);
ok (Math::BigFloat->upgrade()||'','');

Math::BigFloat->div_scale(20); 				# make it a bit faster
my $x = Math::BigFloat->new(2);				# downgrades
# the following test upgrade for bsqrt() and also makes new() NOT downgrade
# for the bpow() side
ok (Math::BigFloat->bpow('2','0.5'),$x->bsqrt());

#require 'upgrade.inc';	# all tests here for sharing
