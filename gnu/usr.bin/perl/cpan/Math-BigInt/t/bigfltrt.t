# -*- mode: perl; -*-

use strict;
use warnings;

use lib 't';

use Test::More tests => 899;

use Math::BigRat::Subclass lib => 'Calc';   # test via this Subclass

our ($CLASS, $LIB);
$CLASS = "Math::BigRat::Subclass";
$LIB   = "Math::BigInt::Calc";

# fails still too many tests
require './t/bigratpm.inc';            # all tests here for sharing
