#!/usr/bin/perl -w

# test BigFloat constants alone (w/o BigInt loading)

use strict;
use Test::More tests => 2;

use Math::BigFloat ':constant';

is (1.0 / 3.0, '0.3333333333333333333333333333333333333333');

# BigInt was not loaded with ':constant', so only floats are handled
is (ref(2 ** 2),'');

