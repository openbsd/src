#!perl -w

# Tests if $$ and getppid return consistent values across threads

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib);
    require './test.pl';
}

use strict;
use Config;

BEGIN {
    if (!$Config{useithreads}) {
	print "1..0 # Skip: no ithreads\n";
	exit;
    }
    if (!$Config{d_getppid}) {
	print "1..0 # Skip: no getppid\n";
	exit;
    }
    eval 'use threads; use threads::shared';
    if ($@ =~ /dynamic loading not available/) {
	print "1..0 # Skip: no dynamic loading, no threads\n";
	exit;
    }
    plan tests => 3;
    if ($@) {
	fail("unable to load thread modules");
    }
    else {
	pass("thread modules loaded");
    }
}

my ($pid, $ppid) = ($$, getppid());
my $pid2 : shared = 0;
my $ppid2 : shared = 0;

new threads( sub { ($pid2, $ppid2) = ($$, getppid()); } ) -> join();

is($pid,  $pid2,  'pids');
is($ppid, $ppid2, 'ppids');
