#!/usr/bin/perl
#	$OpenBSD: direct.pl,v 1.1 2011/01/07 22:06:08 bluhm Exp $

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

use Client;
use Server;
require 'funcs.pl';

our %args;
if (my $test = pop) {
	do $test
	    or die "Do test file $test failed: ", $@ || $!;
}

@ARGV == 0 or die "usage: direct.pl [test-args.pl]\n";

my $s = Server->new(
    func		=> \&read_char,
    %{$args{server}},
    listendomain	=> AF_INET,
    listenaddr		=> "127.0.0.1",
);
my $c = Client->new(
    func		=> \&write_char,
    %{$args{client}},
    connectdomain	=> AF_INET,
    connectaddr		=> "127.0.0.1",
    connectport		=> $s->{listenport},
);

$s->run;
$c->run->up;
$s->up;

$c->down;
$s->down;

exit if $args{nocheck} || $args{client}{nocheck};

my $clen = $c->loggrep(qr/^LEN: /) unless $args{client}{nocheck};
my $slen = $s->loggrep(qr/^LEN: /) unless $args{server}{nocheck};
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

my %name2proc = (client => $c, server => $s);
foreach my $name (qw(client server)) {
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
