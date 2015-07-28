#!/usr/bin/perl
#	$OpenBSD: remote.pl,v 1.6 2015/07/28 12:31:29 bluhm Exp $

# Copyright (c) 2010-2015 Alexander Bluhm <bluhm@openbsd.org>
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

BEGIN {
	if ($> == 0 && $ENV{SUDO_UID}) {
		$> = $ENV{SUDO_UID};
	}
}

use File::Basename;
use File::Copy;
use Socket;
use Socket6;

use Client;
use Server;
use Remote;
require 'funcs.pl';

sub usage {
	die <<"EOF";
usage:
    remote.pl af bindaddr connectaddr connectport test-args.pl
	Only start remote relay.
    remote.pl af bindaddr connectaddr connectport bindport test-args.pl
	Only start remote relay with fixed port, needed for reuse.
    remote.pl af localaddr fakeaddr remotessh test-args.pl
	Run test with local client and server.  Remote relay is
	started automatically with ssh on remotessh.
    remote.pl af localaddr fakeaddr remotessh clientport serverport test-args.pl
	Run test with local client and server and fixed port, needed for reuse.
EOF
}

my $command = "$0 @ARGV";
my $test;
our %args;
if (@ARGV) {
	$test = pop;
	do $test
	    or die "Do test file $test failed: ", $@ || $!;
}
my($af, $domain, $protocol);
if (@ARGV) {
	$af = shift;
	$domain =
	    $af eq "inet" ? AF_INET :
	    $af eq "inet6" ? AF_INET6 :
	    die "address family must be 'inet' or 'inet6\n";
	$protocol = $args{protocol};
	$protocol = $protocol->({ %args, af => $af, domain => $domain, })
	    if ref $protocol eq 'CODE';
}
my $mode =
	@ARGV >= 3 && $ARGV[0] !~ /^\d+$/ && $ARGV[2] =~ /^\d+$/ ? "divert" :
	@ARGV >= 3 && $ARGV[0] !~ /^\d+$/ && $ARGV[2] !~ /^\d+$/ ? "auto"   :
	usage();
my($clientport, $serverport, $bindport);
if (@ARGV == 5 && $mode eq "auto") {
	($clientport, $serverport) = @ARGV[3,4];
} elsif (@ARGV == 4 && $mode eq "divert") {
	($bindport) = $ARGV[3];
} elsif (@ARGV != 3) {
	usage();
}

my($c, $l, $r, $s, $logfile);
my $divert	= $args{divert} || "to";
my $local	= $divert eq "to" ? "client" : "server";
my $remote	= $divert eq "to" ? "server" : "client";
if ($mode eq "divert") {
	$local		= $divert eq "to" ? "server" : "client";
	$remote		= $divert eq "to" ? "client" : "server";
	$logfile	= dirname($0)."/remote.log";
}
my $srcaddr	= $ARGV[0];
my $dstaddr	= $ARGV[1];
if ($mode eq "divert" xor $divert eq "reply") {
	($srcaddr, $dstaddr) = ($dstaddr, $srcaddr);
}

if ($local eq "server") {
	$l = $s = Server->new(
	    %args,
	    %{$args{server}},
	    logfile		=> $logfile,
	    af			=> $af,
	    domain		=> $domain,
	    protocol		=> $protocol,
	    listenaddr		=> $mode ne "divert" ? $ARGV[0] :
		$af eq "inet" ? "127.0.0.1" : "::1",
	    listenport		=> $serverport || $bindport,
	    srcaddr		=> $srcaddr,
	    dstaddr		=> $dstaddr,
	) if $args{server};
}
if ($mode eq "auto") {
	$r = Remote->new(
	    %args,
	    logfile		=> "$remote.log",
	    testfile		=> $test,
	    af			=> $af,
	    remotessh		=> $ARGV[2],
	    bindaddr		=> $ARGV[1],
	    bindport		=> $remote eq "client" ?
		$clientport : $serverport,
	    connect		=> $remote eq "client",
	    connectaddr		=> $ARGV[0],
	    connectport		=> $s ? $s->{listenport} : 0,
	);
	$r->run->up;
	$r->loggrep(qr/^Diverted$/, 10)
	    or die "no Diverted in $r->{logfile}";
}
if ($local eq "client") {
	$l = $c = Client->new(
	    %args,
	    %{$args{client}},
	    logfile		=> $logfile,
	    af			=> $af,
	    domain		=> $domain,
	    protocol		=> $protocol,
	    connectaddr		=> $ARGV[1],
	    connectport		=> $r ? $r->{listenport} : $ARGV[2],
	    bindany		=> $mode eq "divert",
	    bindaddr		=> $ARGV[0],
	    bindport		=> $clientport || $bindport,
	    srcaddr		=> $srcaddr,
	    dstaddr		=> $dstaddr,
	) if $args{client};
}
$l->{log}->print("local command: $command\n") if $l;

if ($mode eq "divert") {
	open(my $log, '<', $l->{logfile})
	    or die "Remote log file open failed: $!";
	$SIG{__DIE__} = sub {
		die @_ if $^S;
		copy($log, \*STDERR);
		warn @_;
		exit 255;
	};
	copy($log, \*STDERR);

	my @cmd = qw(pfctl -a regress -f -);
	my $pf;
	do { local $> = 0; open($pf, '|-', @cmd) }
	    or die "Open pipe to pf '@cmd' failed: $!";
	if ($local eq "server") {
		my $port = $protocol =~ /^(tcp|udp)$/ ?
		    "port $s->{listenport}" : "";
		my $divertport = $port || "port 1";  # XXX bad pf syntax
		print $pf "pass in log $af proto $protocol ".
		    "from $ARGV[1] to $ARGV[0] $port ".
		    "divert-to $s->{listenaddr} $divertport\n";
	} else {
		my $port = $protocol =~ /^(tcp|udp)$/ ?
		    "port $ARGV[2]" : "";
		print $pf "pass out log $af proto $protocol ".
		    "from $c->{bindaddr} to $ARGV[1] $port divert-reply\n";
	}
	close($pf) or die $! ?
	    "Close pipe to pf '@cmd' failed: $!" :
	    "pf '@cmd' failed: $?";
	print STDERR "Diverted\n";

	$l->run;
	copy($log, \*STDERR);
	$l->up;
	copy($log, \*STDERR);
	$l->down;
	copy($log, \*STDERR);

	exit;
}

$s->run if $s;
$c->run->up if $c;
$s->up if $s;

$c->down if $c;
$r->down if $r;
$s->down if $s;

check_logs($c || $r, $s || $r, %args);
