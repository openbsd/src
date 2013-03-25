#!/usr/bin/perl -w

use strict;
use Test::More tests => 686;

use Math::BigRat lib => 'Calc';

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigRat";
$CL = "Math::BigInt::Calc";

require 't/bigratpm.inc';	# all tests here for sharing
