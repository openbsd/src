#!/usr/bin/perl
#	$OpenBSD: remote.pl,v 1.1 2012/12/28 20:36:25 bluhm Exp $

# Copyright (c) 2010-2012 Alexander Bluhm <bluhm@openbsd.org>
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
use Relayd;
use Server;
use Remote;
require 'funcs.pl';

sub usage {
	die <<"EOF";
usage:
    remote.pl localport remoteaddr remoteport [test-args.pl]
	Run test with local client and server.  Remote relayd
	forwarding from remoteaddr remoteport to server localport
	has to be started manually.
    remote.pl copy|splice listenaddr connectaddr connectport [test-args.pl]
	Only start remote relayd.
    remote.pl copy|splice localaddr remoteaddr remotessh [test-args.pl]
	Run test with local client and server.  Remote relayd is
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
	my($rport) = find_ports(num => 1);
	$r = Relayd->new(
	    forward             => $ARGV[0],
	    %{$args{relayd}},
	    listendomain        => AF_INET,
	    listenaddr          => $ARGV[1],
	    listenport          => $rport,
	    connectdomain       => AF_INET,
	    connectaddr         => $ARGV[2],
	    connectport         => $ARGV[3],
	    logfile             => dirname($0)."/remote.log",
	    conffile            => dirname($0)."/relayd.conf",
	    testfile            => $test,
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
	print STDERR "listen sock: $ARGV[1] $rport\n";
	<STDIN>;
	copy($log, \*STDERR);
	print STDERR "stdin closed\n";
	$r->kill_child;
	$r->down;
	copy($log, \*STDERR);

	exit;
}

my $s = Server->new(
    func                => \&read_char,
    %{$args{server}},
    listendomain        => AF_INET,
    listenaddr          => ($mode eq "auto" ? $ARGV[1] : undef),
    listenport          => ($mode eq "manual" ? $ARGV[0] : undef),
);
if ($mode eq "auto") {
	$r = Remote->new(
	    forward             => $ARGV[0],
	    logfile             => "relayd.log",
	    testfile            => $test,
	    %{$args{relay}},
	    remotessh           => $ARGV[3],
	    listenaddr          => $ARGV[2],
	    connectaddr         => $ARGV[1],
	    connectport         => $s->{listenport},
	);
	$r->run->up;
}
my $c = Client->new(
    func                => \&write_char,
    %{$args{client}},
    connectdomain       => AF_INET,
    connectaddr         => ($mode eq "manual" ? $ARGV[1] : $r->{listenaddr}),
    connectport         => ($mode eq "manual" ? $ARGV[2] : $r->{listenport}),
);

$s->run;
$c->run->up;
$s->up;

$c->down;
$s->down;
$r->close_child;
$r->down;

foreach ([ client => $c ], [ relayd => $r ], [ server => $s ]) {
	my($name, $proc) = @$_;
	my $pattern = $args{$name}{loggrep} or next;
	$pattern = [ $pattern ] unless ref($pattern) eq 'ARRAY';
	foreach my $pat (@$pattern) {
		if (ref($pat) eq 'HASH') {
			while (my($re, $num) = each %$pat) {
				my @matches = $proc->loggrep($re);
				@matches == $num or
				    die "$name matches @matches: $re => $num";
			}
		} else {
			$proc->loggrep($pat)
			    or die "$name log missing pattern: $pat";
		}
	}
}

exit if $args{nocheck};

my @clen = $c->loggrep(qr/^LEN: /) or die "no client len"
    unless $args{client}{nocheck};
my @slen = $s->loggrep(qr/^LEN: /) or die "no server len"
    unless $args{server}{nocheck};
!@clen || !@slen || @clen ~~ @slen
    or die "client: @clen", "server: @slen", "len mismatch";
!defined($args{len}) || !$clen[0] || $clen[0] eq "LEN: $args{len}\n"
    or die "client: $clen[0]", "len $args{len} expected";
!defined($args{len}) || !$slen[0] || $slen[0] eq "LEN: $args{len}\n"
    or die "server: $slen[0]", "len $args{len} expected";
foreach my $len (map { ref eq 'ARRAY' ? @$_ : $_ } @{$args{lengths} || []}) {
	my $clen = shift @clen;
	$clen eq "LEN: $len\n"
	    or die "client: $clen", "len $len expected";
	my $slen = shift @slen;
	$slen eq "LEN: $len\n"
	    or die "server: $slen", "len $len expected";
}

my $cmd5 = $c->loggrep(qr/^MD5: /) unless $args{client}{nocheck};
my $smd5 = $s->loggrep(qr/^MD5: /) unless $args{server}{nocheck};
!$cmd5 || !$smd5 || ref($args{md5}) eq 'ARRAY' || $cmd5 eq $smd5
    or die "client: $cmd5", "server: $smd5", "md5 mismatch";
my $md5 = ref($args{md5}) eq 'ARRAY' ? join('|', @{$args{md5}}) : $args{md5};
!$md5 || !$cmd5 || $cmd5 =~ /^MD5: ($md5)$/
    or die "client: $cmd5", "md5 $md5 expected";
!$md5 || !$smd5 || $smd5 =~ /^MD5: ($md5)$/
    or die "server: $smd5", "md5 $md5 expected";
