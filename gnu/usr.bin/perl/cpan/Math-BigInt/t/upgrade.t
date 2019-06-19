#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 2196            # tests in require'd file
                         + 2;           # tests in this file

use Math::BigInt upgrade => 'Math::BigFloat';
use Math::BigFloat;

our ($CLASS, $EXPECTED_CLASS, $CALC);
$CLASS          = "Math::BigInt";
$EXPECTED_CLASS = "Math::BigFloat";
$CALC           = "Math::BigInt::Calc";         # backend

is(Math::BigInt->upgrade(), "Math::BigFloat",
   qq/Math::BigInt->upgrade()/);
is(Math::BigInt->downgrade() || "", "",
   qq/Math::BigInt->downgrade() || ""/);

require 't/upgrade.inc';	# all tests here for sharing
