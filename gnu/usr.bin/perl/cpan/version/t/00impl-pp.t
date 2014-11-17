#! /usr/local/perl -w
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test::More qw/no_plan/;

BEGIN {
    (my $coretests = $0) =~ s'[^/]+\.t'coretests.pm';
    require $coretests;
    use_ok('version::vpp', 0.9909);
}

BaseTests("version::vpp","new","qv");
BaseTests("version::vpp","new","declare");
BaseTests("version::vpp","parse", "qv");
BaseTests("version::vpp","parse", "declare");
