#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 3724            # tests in require'd file
                         + 5;           # tests in this file

use lib 't';

use Math::BigInt::Subclass;

our ($CLASS, $CALC);
$CLASS = "Math::BigInt::Subclass";
$CALC  = "Math::BigInt::Calc";          # backend

require 't/bigintpm.inc';	        # perform same tests as bigintpm

###############################################################################
# Now do custom tests for Subclass itself

my $ms = $CLASS->new(23);
is($ms->{_custom}, 1, '$ms has custom attribute \$ms->{_custom}');

# Check that a subclass is still considered a Math::BigInt
isa_ok($ms, 'Math::BigInt');

use Math::BigInt;

my $bi = Math::BigInt->new(23);		# same as other
$ms += $bi;
is($ms, 46, '$ms is 46');
is($ms->{_custom}, 1, '$ms has custom attribute $ms->{_custom}');
is(ref($ms), $CLASS, "\$ms is not an object of class '$CLASS'");
