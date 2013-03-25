#!/usr/bin/perl -w

# see if using Math::BigInt and Math::BigFloat works together nicely.
# all use_lib*.t should be equivalent

use strict;
use Test::More tests => 2;

BEGIN { unshift @INC, 't'; }

use Math::BigInt lib => 'BareCalc';
use Math::BigFloat;

is (Math::BigInt->config()->{lib},'Math::BigInt::BareCalc');

is (Math::BigFloat->new(123)->badd(123),246);
