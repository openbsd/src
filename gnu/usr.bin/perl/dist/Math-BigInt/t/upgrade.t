#!/usr/bin/perl -w

use strict;
use Test::More tests => 2128
    + 2;			# our own tests

use Math::BigInt upgrade => 'Math::BigFloat';
use Math::BigFloat;

use vars qw ($scale $class $try $x $y $f @args $ans $ans1 $ans1_str $setup
             $ECL $CL);
$class = "Math::BigInt";
$CL = "Math::BigInt::Calc";
$ECL = "Math::BigFloat";

is (Math::BigInt->upgrade(),'Math::BigFloat');
is (Math::BigInt->downgrade()||'','');

require 't/upgrade.inc';	# all tests here for sharing
