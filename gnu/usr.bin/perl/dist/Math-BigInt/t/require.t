#!/usr/bin/perl -w

# check that simple requiring BigInt works

use strict;
use Test::More tests => 1;

my ($x);

require Math::BigInt; $x = Math::BigInt->new(1); ++$x;

is ($x,2);

# all tests done

