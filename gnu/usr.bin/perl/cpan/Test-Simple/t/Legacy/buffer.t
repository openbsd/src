#!/usr/bin/perl
# HARNESS-NO-STREAM

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

# Ensure that intermixed prints to STDOUT and tests come out in the
# right order (ie. no buffering problems).

use Test::More tests => 20;
my $T = Test::Builder->new;
$T->no_ending(1);

for my $num (1..10) {
    $tnum = $num * 2;
    pass("I'm ok");
    $T->current_test($tnum);
    print "ok $tnum - You're ok\n";
}
