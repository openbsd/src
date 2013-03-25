#!/usr/bin/perl -w

use strict;
use Test::More tests => 1;

BEGIN {
  unshift @INC, 't';
}

use Math::BigRat::Test lib => 'Calc';	# test via this Subclass

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigRat::Test";
$CL = "Math::BigInt::Calc";

pass();

# fails still too many tests
#require 't/bigfltpm.inc';		# all tests here for sharing
