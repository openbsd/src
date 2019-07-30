#!perl

# Test use Math::BigFloat with => 'Math::BigInt::SomeSubclass';

use strict;
use warnings;

use Test::More tests => 2409            # tests in require'd file
                         + 1;           # tests in this file

use Math::BigFloat with => 'Math::BigInt::Subclass',
                   lib  => 'Calc';

our ($CLASS, $CALC);
$CLASS = "Math::BigFloat";
$CALC  = "Math::BigInt::Calc";          # backend

# the with argument is ignored
is(Math::BigFloat->config()->{with}, 'Math::BigInt::Calc',
   'Math::BigFloat->config()->{with}');

require 't/bigfltpm.inc';	# all tests here for sharing
