#!./perl -w

# run Porting/bench.pl's selftest

BEGIN {
    @INC = '..' if -f '../TestInit.pm';
}
use TestInit qw(T A); # T is chdir to the top level, A makes paths absolute
use strict;

require 't/test.pl';
my $source = find_git_or_skip('all');
chdir $source or die "Can't chdir to $source: $!";

system "$^X Porting/bench.pl --action=selftest";
