#!/usr/bin/perl -w

# use Module(); doesn't call import() - thanx for cpan testers David. M. Town
# and Andreas Marcel Riechert for spotting it. It is fixed by the same code
# that fixes require Math::BigInt, but we make a test to be sure it really
# works.

use strict;
use Test::More tests => 1;

my ($try,$ans,$x);

use Math::BigInt(); $x = Math::BigInt->new(1); ++$x;

is ($x,2);

# all tests done

1;
