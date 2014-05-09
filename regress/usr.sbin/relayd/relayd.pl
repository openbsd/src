#!/usr/bin/perl
#	$OpenBSD: relayd.pl,v 1.9 2014/05/09 11:49:26 andre Exp $

# Copyright (c) 2010-2013 Alexander Bluhm <bluhm@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;
use Socket;
use Socket6;

use Client;
use Relayd;
use Server;
require 'funcs.pl';

sub usage {
	die "usage: relay.pl copy|splice [test-args.pl]\n";
}

my $test;
our %args;
if (@ARGV and -f $ARGV[-1]) {
	$test = pop;
	do $test
	    or die "Do test file $test failed: ", $@ || $!;
}
@ARGV == 1 or usage();

my($sport, $rport) = find_ports(num => 2);
my $s = Server->new(
    func                => \&read_char,
    listendomain        => AF_INET,
    listenaddr          => "127.0.0.1",
    listenport          => $sport,
    %{$args{server}},
) unless $args{server}{noserver};
my $r = Relayd->new(
    forward             => $ARGV[0],
    listendomain        => AF_INET,
    listenaddr          => "127.0.0.1",
    listenport          => $rport,
    connectdomain       => AF_INET,
    connectaddr         => "127.0.0.1",
    connectport         => $sport,
    %{$args{relayd}},
    testfile            => $test,
);
my $c = Client->new(
    func                => \&write_char,
    connectdomain       => AF_INET,
    connectaddr         => "127.0.0.1",
    connectport         => $rport,
    %{$args{client}},
);

$s->run unless $args{server}{noserver};
$r->run;
$r->up;
$c->run->up unless $args{client}{noclient};
$s->up unless $args{server}{noserver};

$c->down unless $args{client}{noclient};
$s->down unless $args{server}{noserver};
$r->kill_child;
$r->down unless $args{relayd}{dummyrun};

check_logs($c, $r, $s, %args);
