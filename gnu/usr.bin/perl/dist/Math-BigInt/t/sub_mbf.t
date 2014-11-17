#!/usr/bin/perl -w

use strict;
use Test::More tests => 2338
    + 6;	# + our own tests


BEGIN { unshift @INC, 't'; }

use Math::BigFloat::Subclass;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigFloat::Subclass";
$CL = Math::BigFloat->config()->{lib}; # "Math::BigInt::Calc"; or FastCalc

require 't/bigfltpm.inc';	# perform same tests as bigfltpm

###############################################################################
# Now do custom tests for Subclass itself
my $ms = $class->new(23);
print "# Missing custom attribute \$ms->{_custom}" if !is (1, $ms->{_custom});

# Check that subclass is a Math::BigFloat, but not a Math::Bigint
isa_ok ($ms, 'Math::BigFloat');
isnt ($ms->isa('Math::BigInt'), 1);

use Math::BigFloat;

my $bf = Math::BigFloat->new(23);		# same as other
$ms += $bf;
print "# Tried: \$ms += \$bf, got $ms" if !is (46, $ms);
print "# Missing custom attribute \$ms->{_custom}" if !is (1, $ms->{_custom});
print "# Wrong class: ref(\$ms) was ".ref($ms) if !is ($class, ref($ms));
