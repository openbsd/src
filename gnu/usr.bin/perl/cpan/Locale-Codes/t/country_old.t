#!/usr/bin/perl

use warnings;
use strict;
require 5.002;

if ( -f "t/do_tests.pl" ) {
   require "./t/do_tests.pl";
} elsif (-f "do_tests.pl") {
   require "./do_tests.pl";
} else {
  die "ERROR: cannot find do_tests.pl\n";
}

do_tests('country','','old');

1;
# Local Variables:
# mode: cperl
# indent-tabs-mode: nil
# cperl-indent-level: 3
# cperl-continued-statement-offset: 2
# cperl-continued-brace-offset: 0
# cperl-brace-offset: 0
# cperl-brace-imaginary-offset: 0
# cperl-label-offset: 0
# End:
