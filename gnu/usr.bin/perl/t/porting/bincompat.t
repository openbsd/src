#!./perl -w
use strict;
require './test.pl';

use Config;

plan(2);

# Defiantly a white box test...

# As we need to call it direct, we'll take advantage of its result ordering:
my @to_check = qw(bincompat_options non_bincompat_options);
my @V = map {s/^ //r} Internals::V();

while (my ($index, $sub) = each @to_check) {
    my $got = join ' ', sort &{Config->can($sub)}();
    is($got, $V[$index], "C source code has $sub in sorted order");
}
