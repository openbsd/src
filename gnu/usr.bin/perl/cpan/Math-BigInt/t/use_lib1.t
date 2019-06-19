#!/usr/bin/perl

# see if using Math::BigInt and Math::BigFloat works together nicely.
# all use_lib*.t should be equivalent

use strict;
use warnings;

use Test::More tests => 2;

use lib 't';

use Math::BigFloat lib => 'BareCalc';

is(Math::BigInt->config('lib'), 'Math::BigInt::BareCalc',
   "Math::BigInt->config('lib')");

is(Math::BigFloat->new(123)->badd(123), 246,
   'Math::BigFloat->new(123)->badd(123)');
