#!/usr/bin/perl -w

use strict;
use Test::More tests => 3651
    + 5;	# +5 own tests

BEGIN { unshift @INC, 't'; }

use Math::BigInt::Subclass;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt::Subclass";
$CL = "Math::BigInt::Calc";

my $version = '0.02';   # for $VERSION tests, match current release (by hand!)

require 't/bigintpm.inc';	# perform same tests as bigintpm

###############################################################################
# Now do custom tests for Subclass itself

my $ms = $class->new(23);
print "# Missing custom attribute \$ms->{_custom}" if !is (1, $ms->{_custom});

# Check that a subclass is still considered a BigInt
isa_ok ($ms, 'Math::BigInt');

use Math::BigInt;

my $bi = Math::BigInt->new(23);		# same as other
$ms += $bi;
print "# Tried: \$ms += \$bi, got $ms" if !is (46, $ms);
print "# Missing custom attribute \$ms->{_custom}" if !is (1, $ms->{_custom});
print "# Wrong class: ref(\$ms) was ".ref($ms) if !is ($class, ref($ms));
