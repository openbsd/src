#!/usr/bin/perl -w

# check that simple requiring BigFloat and then new() works

use strict;
use Test::More tests => 1;

require Math::BigFloat; my $x = Math::BigFloat->new(1);  ++$x; is ($x,2);

# all tests done
