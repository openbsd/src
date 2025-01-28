# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 899;           # tests in require'd file

use lib 't';

use Math::BigRat lib => 'BareCalc';

print "# ", Math::BigRat->config('lib'), "\n";

our ($CLASS, $LIB);
$CLASS = "Math::BigRat";
$LIB   = "Math::BigInt::BareCalc";      # backend

require './t/bigratpm.inc';               # perform same tests as bigratpm.t
