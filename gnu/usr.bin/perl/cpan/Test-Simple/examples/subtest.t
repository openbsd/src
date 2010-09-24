#!/usr/bin/env perl

use strict;
use warnings;

use lib '../lib';
use Test::More tests => 3;

ok 1;
subtest 'some name' => sub {
    my $num_tests = 2 + int( rand(3) );
    plan tests => $num_tests;
    ok 1 for 1 .. $num_tests - 1;
    subtest 'some name' => sub {
        plan 'no_plan';
        ok 1 for 1 .. 2 + int( rand(3) );
    };
};
ok 1;
