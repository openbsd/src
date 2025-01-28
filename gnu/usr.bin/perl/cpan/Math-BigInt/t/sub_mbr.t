# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 899             # tests in require'd file
                        + 9;            # tests in this file

use lib 't';

use Math::BigRat::Subclass;

our ($CLASS, $LIB);
$CLASS = "Math::BigRat::Subclass";
$LIB   = $CLASS -> config('lib');       # backend library

require './t/bigratpm.inc';

###############################################################################
# Now do custom tests for Subclass itself

my $ms = $CLASS -> new(23);
is($ms->{_custom}, 1, '$ms has custom attribute \$ms->{_custom}');

# Check that a subclass is still considered a Math::BigRat
isa_ok($ms, 'Math::BigRat');

my $bi = Math::BigRat -> new(23);         # same as other
$ms += $bi;
is($ms, 46, '$ms is 46');
is($ms->{_custom}, 1, '$ms has custom attribute $ms->{_custom}');
is(ref($ms), $CLASS, "\$ms is not an object of class '$CLASS'");

is($CLASS -> accuracy(), undef,
   "$CLASS gets 'accuracy' from parent");

is($CLASS -> precision(), undef,
   "$CLASS gets 'precision' from parent");

cmp_ok($CLASS -> div_scale(), "==", 40,
       "$CLASS gets 'div_scale' from parent");

is($CLASS -> round_mode(), "even",
   "$CLASS gets 'round_mode' from parent");
