#!perl

use strict;
use warnings;

use Test::More tests => 2482;

use lib 't';

use Math::BigFloat lib => 'BareCalc';

our ($CLASS, $CALC);
$CLASS = "Math::BigFloat";
$CALC  = "Math::BigInt::BareCalc";      # backend

require 't/bigfltpm.inc';	        # all tests here for sharing
