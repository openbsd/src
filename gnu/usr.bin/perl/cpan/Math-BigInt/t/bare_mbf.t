#!perl

use strict;
use warnings;

use Test::More tests => 2830;

use lib 't';

use Math::BigFloat lib => 'BareCalc';

our ($CLASS, $LIB);
$CLASS = "Math::BigFloat";
$LIB   = "Math::BigInt::BareCalc";      # backend

require './t/bigfltpm.inc';             # all tests here for sharing
