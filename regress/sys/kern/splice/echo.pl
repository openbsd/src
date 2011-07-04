#!/usr/bin/perl
#	$OpenBSD: echo.pl,v 1.3 2011/07/04 05:43:02 bluhm Exp $

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
use Socket;
use Socket6;

use Child;
use Client;
use Server;
require 'funcs.pl';

sub usage {
	die "usage: echo.pl copy|splice [test-args.pl]\n";
}

my $test;
our %args;
if (@ARGV and -f $ARGV[-1]) {
	$test = pop;
	do $test
	    or die "Do test file $test failed: ", $@ || $!;
}
@ARGV == 1 or usage();

exit 0 if $args{noecho};

my $r = Server->new(
    forward		=> $ARGV[0],
    func		=> \&relay,
    logfile		=> "relay.log",
    listendomain	=> AF_INET,
    listenaddr		=> "127.0.0.1",
    %{$args{relay}},
);
my $s = Child->new(
    logfile		=> "server.log",
    oobinline		=> 1,
    %{$args{server}},
    func		=> sub {
	($args{server}{func} || \&read_char)->(@_);
	eval { shutout(@_) };
    },
);
my $c = Client->new(
    connectdomain	=> AF_INET,
    connectaddr		=> "127.0.0.1",
    connectport		=> $r->{listenport},
    oobinline		=> 1,
    %{$args{client}},
    func		=> sub {
	$s->run->up;
	eval { ($args{client}{func} || \&write_char)->(@_) };
	warn $@ if $@;
	eval { shutout(@_) };
	$s->down;
    },
);

$r->run;
$c->run->up;
$r->up;

$c->down;
$r->down;
$s->{pid} = -1;  # XXX hack

exit if $args{nocheck};

$r->loggrep(qr/^Timeout$/) or die "no relay timeout"
    if $args{relay}{idle};
$r->loggrep(qr/^Max$/) or die "no relay max"
    if $args{relay}{max} && $args{len};

my $clen = $c->loggrep(qr/^LEN: /) // die "no client len"
    unless $args{client}{nocheck};
my $rlen = $r->loggrep(qr/^LEN: /) // die "no relay len"
    unless $args{relay}{nocheck};
my $slen = $s->loggrep(qr/^LEN: /) // die "no server len"
    unless $args{server}{nocheck};
!$clen || !$rlen || $clen eq $rlen
    or die "client: $clen", "relay: $rlen", "len mismatch";
!$rlen || !$slen || $rlen eq $slen
    or die "relay: $rlen", "server: $slen", "len mismatch";
!$clen || !$slen || $clen eq $slen
    or die "client: $clen", "server: $slen", "len mismatch";
!defined($args{len}) || !$clen || $clen eq "LEN: $args{len}\n"
    or die "client: $clen", "len $args{len} expected";
!defined($args{len}) || !$rlen || $rlen eq "LEN: $args{len}\n"
    or die "relay: $rlen", "len $args{len} expected";
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
$args{relay}{errorin} //= $args{relay}{error};
if (defined($args{relay}{errorin})) {
	my $ein = $r->loggrep(qr/^ERROR IN: /);
	defined($ein) && $ein eq "ERROR IN: $args{relay}{errorin}\n"
	    or die "relay: $ein",
	    "error in $args{relay}{errorin} expected";
}
if (defined($args{relay}{errorout})) {
	my $eout = $r->loggrep(qr/^ERROR OUT: /);
	defined($eout) && $eout eq "ERROR OUT: $args{relay}{errorout}\n"
	    or die "relay: $eout",
	    "error out $args{relay}{errorout} expected";
}
