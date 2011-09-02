#!/usr/bin/perl
#	$OpenBSD: relayd.pl,v 1.3 2011/09/02 21:05:41 bluhm Exp $

# Copyright (c) 2010,2011 Alexander Bluhm <bluhm@openbsd.org>
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
);
my $r = Relayd->new(
    forward             => $ARGV[0],
    listendomain        => AF_INET,
    listenaddr          => "127.0.0.1",
    listenport          => $rport,
    connectdomain       => AF_INET,
    connectaddr         => "127.0.0.1",
    connectport         => $sport,
    %{$args{relayd}},
    test                => $test,
);
my $c = Client->new(
    func                => \&write_char,
    connectdomain       => AF_INET,
    connectaddr         => "127.0.0.1",
    connectport         => $rport,
    %{$args{client}},
);

$s->run;
$r->run;
$r->up;
$c->run->up;
$s->up;

$c->down;
$s->down;
$r->down;

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
foreach my $len (@{$args{lengths} || []}) {
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
