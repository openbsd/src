#!perl

use strict;
use warnings;

use Test::More tests => 4;

use lib 't';

# first load Math::BigInt with Math::BigInt::Calc
use Math::BigInt lib => 'Calc';

# Math::BigFloat will remember that we loaded Math::BigInt::Calc
require Math::BigFloat;
is(Math::BigFloat->config("lib"), 'Math::BigInt::Calc',
   'Math::BigFloat got Math::BigInt::Calc');

# now load Math::BigInt again with a different lib
Math::BigInt->import(lib => 'BareCalc');

# and finally test that Math::BigFloat knows about Math::BigInt::BareCalc

is(Math::BigFloat->config("lib"), 'Math::BigInt::BareCalc',
   'Math::BigFloat was notified');

# See that Math::BigFloat supports "only"
eval { Math::BigFloat->import('only' => 'Calc') };
is(Math::BigFloat->config("lib"), 'Math::BigInt::Calc', '"only" worked');

# See that Math::BigFloat supports "try"
eval { Math::BigFloat->import('try' => 'BareCalc') };
is(Math::BigFloat->config("lib"), 'Math::BigInt::BareCalc', '"try" worked');
