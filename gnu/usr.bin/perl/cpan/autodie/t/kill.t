#!/usr/bin/perl -w
use strict;
use Test::More;
use autodie;

use constant SYSINIT => 1;

if (not CORE::kill(0,$$)) {
    plan skip_all => "Can't send signals to own process on this system.";
}

if (CORE::kill(0, SYSINIT)) {
    plan skip_all => "Can unexpectedly signal process 1. Won't run as root.";
}

plan tests => 4;

eval { kill(0, $$); };
is($@, '', "Signalling self is fine");

eval { kill(0, SYSINIT ) };
isa_ok($@, 'autodie::exception', "Signalling init is not allowed.");

eval { kill(0, $$, SYSINIT) };
isa_ok($@, 'autodie::exception', 'kill exception on single failure.');
is($@->return, 1, "kill fails correctly on a 'true' failure.");
