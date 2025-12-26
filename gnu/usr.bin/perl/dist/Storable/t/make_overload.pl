#!/usr/bin/env perl
use strict;
use warnings;

use Storable qw(nfreeze);
use HAS_OVERLOAD;

my $o = HAS_OVERLOAD->make("snow");
my $f = nfreeze \$o;

my $uu = pack 'u', $f;

print $uu;

