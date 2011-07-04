#!/usr/bin/perl
#	$OpenBSD: remote.pl,v 1.3 2011/07/04 05:43:02 bluhm Exp $

# Copyright (c) 2010 Alexander Bluhm <bluhm@openbsd.org>
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
use Relay;
use Server;
use Remote;
require 'funcs.pl';

sub usage {
	die <<"EOF";
usage:
    remote.pl localport remoteaddr remoteport [test-args.pl]
	Run test with local client and server.  Remote relay
	forwarding from remoteaddr remoteport to server localport
	has to be started manually.
    remote.pl copy|splice listenaddr connectaddr connectport [test-args.pl]
	Only start remote relay.
    remote.pl copy|splice localaddr remoteaddr remotessh [test-args.pl]
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
my $mode =
	@ARGV == 3 && $ARGV[0] =~ /^\d+$/ && $ARGV[2] =~ /^\d+$/ ? "manual" :
	@ARGV == 4 && $ARGV[1] !~ /^\d+$/ && $ARGV[3] =~ /^\d+$/ ? "relay"  :
	@ARGV == 4 && $ARGV[1] !~ /^\d+$/ && $ARGV[3] !~ /^\d+$/ ? "auto"   :
	usage();

my $r;
if ($mode eq "relay") {
	$r = Relay->new(
	    forward		=> $ARGV[0],
	    logfile		=> dirname($0)."/remote.log",
	    func		=> \&relay,
	    %{$args{relay}},
	    listendomain	=> AF_INET,
	    listenaddr		=> $ARGV[1],
	    connectdomain	=> AF_INET,
	    connectaddr		=> $ARGV[2],
	    connectport		=> $ARGV[3],
	);
	open(my $log, '<', $r->{logfile})
	    or die "Remote log file open failed: $!";
	$SIG{__DIE__} = sub {
		die @_ if $^S;
		copy($log, \*STDERR);
		warn @_;
		exit 255;
	};
	copy($log, \*STDERR);
	$r->run;
	copy($log, \*STDERR);
	$r->up;
	copy($log, \*STDERR);
	$r->down;
	copy($log, \*STDERR);

	exit;
}

my $s = Server->new(
    func		=> \&read_char,
    oobinline		=> 1,
    %{$args{server}},
    listendomain	=> AF_INET,
    listenaddr		=> ($mode eq "auto" ? $ARGV[1] : undef),
    listenport		=> ($mode eq "manual" ? $ARGV[0] : undef),
);
if ($mode eq "auto") {
	$r = Remote->new(
	    forward		=> $ARGV[0],
	    logfile		=> "relay.log",
	    testfile		=> $test,
	    %{$args{relay}},
	    remotessh		=> $ARGV[3],
	    listenaddr		=> $ARGV[2],
	    connectaddr		=> $ARGV[1],
	    connectport		=> $s->{listenport},
	);
	$r->run->up;
}
my $c = Client->new(
    func		=> \&write_char,
    oobinline		=> 1,
    %{$args{client}},
    connectdomain	=> AF_INET,
    connectaddr		=> ($mode eq "manual" ? $ARGV[1] : $r->{listenaddr}),
    connectport		=> ($mode eq "manual" ? $ARGV[2] : $r->{listenport}),
);

$s->run;
$c->run->up;
$s->up;

$c->down;
$r->down if $r;
$s->down;

exit if $args{nocheck};

$r->loggrep(qr/^Timeout$/) or die "no relay timeout"
    if $args{relay}{idle};
$r->loggrep(qr/^Max$/) or die "no relay max"
    if $args{relay}{max} && $args{len};

my $clen = $c->loggrep(qr/^LEN: /) // die "no client len"
    unless $args{client}{nocheck};
my $slen = $s->loggrep(qr/^LEN: /) // die "no server len"
    unless $args{server}{nocheck};
!$clen || !$slen || $clen eq $slen
    or die "client: $clen", "server: $slen", "len mismatch";
!defined($args{len}) || !$clen || $clen eq "LEN: $args{len}\n"
    or die "client: $clen", "len $args{len} expected";
!defined($args{len}) || !$slen || $slen eq "LEN: $args{len}\n"
    or die "server: $slen", "len $args{len} expected";

my $cmd5 = $c->loggrep(qr/^MD5: /) unless $args{client}{nocheck};
my $smd5 = $s->loggrep(qr/^MD5: /) unless $args{server}{nocheck};
!$cmd5 || !$smd5 || $cmd5 eq $smd5
    or die "client: $cmd5", "server: $smd5", "md5 mismatch";
!defined($args{md5}) || !$cmd5 || $cmd5 eq "MD5: $args{md5}\n"
    or die "client: $cmd5", "md5 $args{md5} expected";
!defined($args{md5}) || !$smd5 || $smd5 eq "MD5: $args{md5}\n"
    or die "server: $smd5", "md5 $args{md5} expected";

$args{relay}{errorin} //= 0 unless $args{relay}{nocheck};
$args{relay}{errorout} //= 0 unless $args{relay}{nocheck};
my %name2proc = (client => $c, relay => $r, server => $s);
foreach my $name (qw(client relay server)) {
	$args{$name}{errorin} //= $args{$name}{error};
	if (defined($args{$name}{errorin})) {
		my $ein = $name2proc{$name}->loggrep(qr/^ERROR IN: /);
		defined($ein) && $ein eq "ERROR IN: $args{$name}{errorin}\n"
		    or die "$name: $ein",
		    "error in $args{$name}{errorin} expected";
	}
	if (defined($args{$name}{errorout})) {
		my $eout = $name2proc{$name}->loggrep(qr/^ERROR OUT: /);
		defined($eout) && $eout eq "ERROR OUT: $args{$name}{errorout}\n"
		    or die "$name: $eout",
		    "error out $args{$name}{errorout} expected";
	}
}
