package main;
use strict;
use warnings;

# Calls open, while still in the main package.  This shouldn't
# be autodying.
sub leak_test {
    return open(my $fh, '<', $_[0]);
}

package autodie_test_module;

# This should be calling CORE::open
sub your_open {
    return open(my $fh, '<', $_[0]);
}

1;
