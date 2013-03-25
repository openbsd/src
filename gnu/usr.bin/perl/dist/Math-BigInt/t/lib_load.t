#!/usr/bin/perl -w

use strict;
use Test::More tests => 4;

BEGIN { unshift @INC, 't'; }

# first load BigInt with Calc
use Math::BigInt lib => 'Calc';

# BigFloat will remember that we loaded Calc
require Math::BigFloat;
is (Math::BigFloat::config()->{lib}, 'Math::BigInt::Calc', 'BigFloat got Calc');

# now load BigInt again with a different lib
Math::BigInt->import( lib => 'BareCalc' );

# and finally test that BigFloat knows about BareCalc

is (Math::BigFloat::config()->{lib}, 'Math::BigInt::BareCalc', 'BigFloat was notified');

# See that Math::BigFloat supports "only"
eval "Math::BigFloat->import('only' => 'Calc')";
is (Math::BigFloat::config()->{lib}, 'Math::BigInt::Calc', '"only" worked');

# See that Math::BigFloat supports "try"
eval "Math::BigFloat->import('try' => 'BareCalc')";
is (Math::BigFloat::config()->{lib}, 'Math::BigInt::BareCalc', '"try" worked');

