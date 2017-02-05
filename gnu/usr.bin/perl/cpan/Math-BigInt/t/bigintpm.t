#!perl

use strict;
use warnings;

use Test::More tests => 3724            # tests in require'd file
                         + 6;           # tests in this file

use Math::BigInt lib => 'Calc';

our ($CLASS, $CALC);
$CLASS = "Math::BigInt";
$CALC  = "Math::BigInt::Calc";

my $x;

#############################################################################
# from_hex(), from_bin() and from_oct() tests

$x = Math::BigInt->from_hex('0xcafe');
is($x, "51966",
   qq|Math::BigInt->from_hex("0xcafe")|);

$x = Math::BigInt->from_hex('0xcafebabedead');
is($x, "223195403574957",
   qq|Math::BigInt->from_hex("0xcafebabedead")|);

$x = Math::BigInt->from_bin('0b1001');
is($x, "9",
   qq|Math::BigInt->from_bin("0b1001")|);

$x = Math::BigInt->from_bin('0b1001100110011001100110011001');
is($x, "161061273",
   qq|Math::BigInt->from_bin("0b1001100110011001100110011001");|);

$x = Math::BigInt->from_oct('0775');
is($x, "509",
   qq|Math::BigInt->from_oct("0775");|);

$x = Math::BigInt->from_oct('07777777777777711111111222222222');
is($x, "9903520314281112085086151826",
   qq|Math::BigInt->from_oct("07777777777777711111111222222222");|);

#############################################################################
# all the other tests

require 't/bigintpm.inc';       # all tests here for sharing
