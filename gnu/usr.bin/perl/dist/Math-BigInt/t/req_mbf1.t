#!/usr/bin/perl -w 

# check that simple requiring BigFloat and then bone() works

use strict;
use Test::More tests => 1;

require Math::BigFloat; my $x = Math::BigFloat->bone(); is ($x,1);

# all tests done
