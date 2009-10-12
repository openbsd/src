#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use Config;
plan tests => 2;

SKIP: {
    skip "setpgrp() is not available", 2 unless $Config{d_setpgrp};
    ok(!eval { package A;sub foo { die("got here") }; package main; A->foo(setpgrp())});
    ok($@ =~ /got here/, "setpgrp() should extend the stack before modifying it");
}
