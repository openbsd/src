use strict;
use warnings;
use Test2::Tools::Tiny;

BEGIN { skip_all "Only testing on 5.18+" if $] < 5.018 }

require Test::More;
*cmp_ok = \&Test::More::cmp_ok;

no warnings "experimental::smartmatch";

my $warnings = warnings { cmp_ok(1, "~~", 1) };

ok(!@$warnings, "Did not get any warnings");

done_testing;
