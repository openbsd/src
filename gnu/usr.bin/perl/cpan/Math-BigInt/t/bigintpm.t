#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  unshift @INC, '../lib'; # for running manually
  my $location = $0; $location =~ s/bigintpm.t//;
  unshift @INC, $location; # to locate the testing files
  chdir 't' if -d 't';
  plan tests => 3273 + 6;
  }

use Math::BigInt lib => 'Calc';

use vars qw ($scale $class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt";
$CL = "Math::BigInt::Calc";

#############################################################################
# from_hex(), from_bin() and from_oct() tests

my $x = Math::BigInt->from_hex('0xcafe');
ok ($x, "51966", 'from_hex() works');
 
$x = Math::BigInt->from_hex('0xcafebabedead');
ok ($x, "223195403574957", 'from_hex() works with long numbers');
 
$x = Math::BigInt->from_bin('0b1001');
ok ($x, "9", 'from_bin() works');
 
$x = Math::BigInt->from_bin('0b1001100110011001100110011001');
ok ($x, "161061273", 'from_bin() works with big numbers');

$x = Math::BigInt->from_oct('0775');
ok ($x, "509", 'from_oct() works');
 
$x = Math::BigInt->from_oct('07777777777777711111111222222222');
ok ($x, "9903520314281112085086151826", 'from_oct() works with big numbers');

#############################################################################
# all the other tests
 
require 'bigintpm.inc';	# all tests here for sharing
