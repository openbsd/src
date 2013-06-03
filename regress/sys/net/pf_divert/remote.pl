#!/usr/bin/perl
#	$OpenBSD: remote.pl,v 1.1.1.1 2013/06/03 05:06:38 bluhm Exp $

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
    remote.pl af bindaddr connectaddr connectport [test-args.pl]
	Only start remote relay.
    remote.pl af localaddr fakeaddr remotessh [test-args.pl]
	Run test with local client and server.  Remote relay is
	started automatically with ssh on remotessh.
EOF
}

my $test;
our %args;
if (@ARGV and -f $ARGV[-1]) {
	$test = pop;
	do $test
	    or die "Do test file $test failed: ", $@ || $!;
}
my($af, $domain);
if (@ARGV) {
	$af = shift;
	$domain =
	    $af eq "inet" ? AF_INET :
	    $af eq "inet6" ? AF_INET6 :
	    die "address family must be 'inet' or 'inet6\n";
}
my $mode =
	@ARGV == 3 && $ARGV[0] !~ /^\d+$/ && $ARGV[2] =~ /^\d+$/ ? "divert"  :
	@ARGV == 3 && $ARGV[0] !~ /^\d+$/ && $ARGV[2] !~ /^\d+$/ ? "auto"   :
	usage();

my($c, $l, $r, $s, $logfile);
my $func = \&write_read_stream;
my $divert = $args{divert} || "to";
my $local = $divert eq "to" ? "client" : "server";
my $remote = $divert eq "to" ? "server" : "client";
if ($mode eq "divert") {
	$local		= $divert eq "to" ? "server" : "client";
	$logfile	= dirname($0)."/remote.log";
}

if ($local eq "server") {
	$l = $s = Server->new(
	    func		=> $func,
	    %{$args{server}},
	    logfile		=> $logfile,
	    listendomain	=> $domain,
	    listenaddr		=> $mode ne "divert" ? $ARGV[0] :
		$af eq "inet" ? "127.0.0.1" : "::1",
	);
}
if ($mode eq "auto") {
	$r = Remote->new(
	    %{$args{relay}},
	    af			=> $af,
	    logfile		=> "$remote.log",
	    testfile		=> $test,
	    remotessh		=> $ARGV[2],
	    bindaddr		=> $ARGV[1],
	    connectaddr		=> $ARGV[0],
	    connectport		=> $s ? $s->{listenport} : 0,
	    sudo		=> $ENV{SUDO},
	);
	$r->run->up;
	$r->loggrep(qr/^Diverted$/, 10)
	    or die "no Diverted in $r->{logfile}";
}
if ($local eq "client") {
	$l = $c = Client->new(
	    func		=> $func,
	    %{$args{client}},
	    logfile		=> $logfile,
	    connectdomain	=> $domain,
	    connectaddr		=> $ARGV[1],
	    connectport		=> $r ? $r->{listenport} : $ARGV[2],
	    bindany		=> $mode eq "divert",
	    bindaddr		=> $ARGV[0],
	);
}

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

	my @sudo = $ENV{SUDO} ? $ENV{SUDO} : ();
	my @cmd = (@sudo, qw(pfctl -a regress -f -));
	open(my $pf, '|-', @cmd)
	    or die "Open pipe to pf '@cmd' failed: $!";
	if ($local eq "server") {
		print $pf "pass in log $af proto tcp ".
		    "from $ARGV[1] to $ARGV[0] port $s->{listenport} ".
		    "divert-to $s->{listenaddr} port $s->{listenport}\n";
	} else {
		print $pf "pass out log $af proto tcp ".
		    "from $c->{bindaddr} to $ARGV[1] port $ARGV[2] ".
		    "divert-reply\n";
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

check_logs($c, $s, %args);
