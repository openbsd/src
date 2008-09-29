#!perl -w

# test per-interpeter static data API (MY_CXT)
# DAPM Dec 2005

my $threads;
BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bXS\/APItest\b/) {
	# Look, I'm using this fully-qualified variable more than once!
	my $arch = $MacPerl::Architecture;
        print "1..0 # Skip: XS::APItest was not built\n";
        exit 0;
    }
    $threads = $Config{'useithreads'};
    # must 'use threads' before 'use Test::More'
    eval 'use threads' if $threads;
}

use warnings;
use strict;

use Test::More tests => 11;

BEGIN {
    use_ok('XS::APItest');
};

is(my_cxt_getint(), 99, "initial int value");
is(my_cxt_getsv(),  "initial", "initial SV value");

my_cxt_setint(1234);
is(my_cxt_getint(), 1234, "new int value");

my_cxt_setsv("abcd");
is(my_cxt_getsv(),  "abcd", "new SV value");

sub do_thread {
    is(my_cxt_getint(), 1234, "initial int value (child)");
    my_cxt_setint(4321);
    is(my_cxt_getint(), 4321, "new int value (child)");

    is(my_cxt_getsv(), "initial_clone", "initial sv value (child)");
    my_cxt_setsv("dcba");
    is(my_cxt_getsv(),  "dcba", "new SV value (child)");
}

SKIP: {
    skip "No threads", 4 unless $threads;
    threads->create(\&do_thread)->join;
}

is(my_cxt_getint(), 1234,  "int value preserved after join");
is(my_cxt_getsv(),  "abcd", "SV value preserved after join");
