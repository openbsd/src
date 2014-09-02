#!/usr/bin/perl
#	$OpenBSD: syslogd.pl,v 1.2 2014/09/02 00:26:30 bluhm Exp $

# Copyright (c) 2010-2014 Alexander Bluhm <bluhm@openbsd.org>
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
use Syslogd;
use Server;
require 'funcs.pl';

sub usage {
	die "usage: syslogd.pl [test-args.pl]\n";
}

my $testfile;
our %args;
if (@ARGV and -f $ARGV[-1]) {
	$testfile = pop;
	do $testfile
	    or die "Do test file $testfile failed: ", $@ || $!;
}
@ARGV == 0 or usage();

foreach my $name (qw(client syslogd server)) {
	foreach my $action (qw(connect listen)) {
		my $h = $args{$name}{$action} or next;
		foreach my $k (qw(protocol domain addr port)) {
			$args{$name}{"$action$k"} = $h->{$k};
		}
	}
}
my($s, $c, $r);
$s = Server->new(
    func                => \&read_log,
    listendomain        => AF_INET,
    listenaddr          => "127.0.0.1",
    %{$args{server}},
    testfile            => $testfile,
    client              => \$c,
    syslogd             => \$r,
) unless $args{server}{noserver};
$r = Syslogd->new(
    connectaddr         => "127.0.0.1",
    connectport         => $s && $s->{listenport},
    %{$args{syslogd}},
    testfile            => $testfile,
    client              => \$c,
    server              => \$s,
);
$c = Client->new(
    func                => \&write_log,
    %{$args{client}},
    testfile            => $testfile,
    syslogd             => \$r,
    server              => \$s,
) unless $args{client}{noclient};

$r->run;
$s->run->up unless $args{server}{noserver};
$r->up;
$c->run->up unless $args{client}{noclient};

$c->down unless $args{client}{noclient};
$s->down unless $args{server}{noserver};
$r->kill_child;
$r->down;

check_logs($c, $r, $s, %args);
$args{check}->({client => $c, syslogd => $r, server => $s}) if $args{check};
