package B::Lint::Plugin::Test;
use strict;
use warnings;

# This package will be loaded automatically by Module::Plugin when
# B::Lint loads.
warn 'got here!';

sub match {
    my $op = shift @_;

    # Prints to STDERR which will be picked up by the test running in
    # lint.t
    warn "Module::Pluggable ok.\n";

    # Ignore this method once it happens once.
    *match = sub { };
}

1;
