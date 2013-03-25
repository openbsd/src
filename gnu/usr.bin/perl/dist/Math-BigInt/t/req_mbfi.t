#!/usr/bin/perl -w 

# check that simple requiring BigFloat and then binf() works

use strict;
use Test::More tests => 1;

require Math::BigFloat; my $x = Math::BigFloat->binf(); is ($x,'inf');

# all tests done
