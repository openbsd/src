#!/usr/bin/perl -w

use strict;
use Test::More tests => 3651 + 6;

use Math::BigInt lib => 'Calc';

use vars qw ($scale $class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt";
$CL = "Math::BigInt::Calc";

#############################################################################
# from_hex(), from_bin() and from_oct() tests

my $x = Math::BigInt->from_hex('0xcafe');
is ($x, "51966", 'from_hex() works');
 
$x = Math::BigInt->from_hex('0xcafebabedead');
is ($x, "223195403574957", 'from_hex() works with long numbers');
 
$x = Math::BigInt->from_bin('0b1001');
is ($x, "9", 'from_bin() works');
 
$x = Math::BigInt->from_bin('0b1001100110011001100110011001');
is ($x, "161061273", 'from_bin() works with big numbers');

$x = Math::BigInt->from_oct('0775');
is ($x, "509", 'from_oct() works');
 
$x = Math::BigInt->from_oct('07777777777777711111111222222222');
is ($x, "9903520314281112085086151826", 'from_oct() works with big numbers');

#############################################################################
# all the other tests
 
require 't/bigintpm.inc';	# all tests here for sharing
