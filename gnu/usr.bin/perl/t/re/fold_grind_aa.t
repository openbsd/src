#!./perl

# Call fold_grind with /aa

use strict;
use warnings;
no warnings 'once';

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    require './loc_tools.pl';
    set_up_inc('../lib');
}

$::TEST_CHUNK = 'aa';

do './re/fold_grind.pl';
print STDERR "$@\n" if $@;
print STDERR "$!\n" if $!;
