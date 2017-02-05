#!perl

# test Math::BigFloat constants alone (w/o Math::BigInt loading)

use strict;
use warnings;

use Test::More tests => 2;

use Math::BigFloat ':constant';

is(1.0 / 3.0, '0.3333333333333333333333333333333333333333',
   "1.0 / 3.0 = 0.3333333333333333333333333333333333333333");

# Math::BigInt was not loaded with ':constant', so only floats are handled
is(ref(2 ** 2), '', "2 ** 2 is a scalar");
