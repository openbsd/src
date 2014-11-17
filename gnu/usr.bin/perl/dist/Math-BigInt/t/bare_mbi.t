#!/usr/bin/perl -w

use strict;
use Test::More tests => 3651;

BEGIN { unshift @INC, 't'; }

use Math::BigInt lib => 'BareCalc';

print "# ",Math::BigInt->config()->{lib},"\n";

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt";
$CL = "Math::BigInt::BareCalc";

my $version = '1.84';	# for $VERSION tests, match current release (by hand!)

require 't/bigintpm.inc';	# perform same tests as bigintpm
