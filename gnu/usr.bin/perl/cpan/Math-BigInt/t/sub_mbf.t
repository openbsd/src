#!perl

use strict;
use warnings;

use Test::More tests => 2830            # tests in require'd file
                         + 6;           # tests in this file

use lib 't';

use Math::BigFloat::Subclass;

our ($CLASS, $LIB);
$CLASS = "Math::BigFloat::Subclass";
$LIB   = Math::BigFloat->config('lib');         # backend library

require './t/bigfltpm.inc';     # perform same tests as bigfltpm

###############################################################################
# Now do custom tests for Subclass itself

my $ms = $CLASS->new(23);
is($ms->{_custom}, 1, '$ms has custom attribute \$ms->{_custom}');

# Check that subclass is a Math::BigFloat, but not a Math::Bigint
isa_ok($ms, 'Math::BigFloat');
ok(!$ms->isa('Math::BigInt'),
   "An object of class '" . ref($ms) . "' isn't a 'Math::BigFloat'");

use Math::BigFloat;

my $bf = Math::BigFloat->new(23);       # same as other
$ms += $bf;
is($ms, 46, '$ms is 46');
is($ms->{_custom}, 1, '$ms has custom attribute $ms->{_custom}');
is(ref($ms), $CLASS, "\$ms is not an object of class '$CLASS'");
