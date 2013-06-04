#	$OpenBSD: funcs.pl,v 1.3 2013/06/04 04:17:42 bluhm Exp $

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

########################################################################
# Client and Server funcs
########################################################################

sub write_read_stream {
	my $self = shift;

	my $out = ref($self). "\n";
	print $out;
	IO::Handle::flush(\*STDOUT);
	print STDERR ">>> $out";

	my $in = <STDIN>;
	print STDERR "<<< $in";
}

sub write_datagram {
	my $self = shift;

	my $out = ref($self). "\n";
	print $out;
	IO::Handle::flush(\*STDOUT);
	print STDERR ">>> $out";
}

sub read_datagram {
	my $self = shift;
	my $skip = $self->{skip};
	$skip = $skip->($self) if ref $skip eq 'CODE';

	my $in;
	if ($skip) {
		# Raw sockets include the IPv4 header.
		sysread(STDIN, $in, 70000);
		# Cut the header off.
		substr($in, 0, $skip, "");
	} else {
		$in = <STDIN>;
	}
	print STDERR "<<< $in";
}

########################################################################
# Script funcs
########################################################################

sub check_logs {
	my ($c, $s, %args) = @_;

	return if $args{nocheck};

	check_inout($c, $s, %args);
}

sub check_inout {
	my ($c, $s, %args) = @_;

	if ($c && !$args{client}{nocheck}) {
		$c->loggrep(qr/^>>> Client$/) or die "no client output"
		    unless $args{client}{noout};
		$c->loggrep(qr/^<<< Server$/) or die "no client input"
		    unless $args{client}{noin};
	}
	if ($s && !$args{server}{nocheck}) {
		$s->loggrep(qr/^>>> Server$/) or die "no server output"
		    unless $args{server}{noout};
		$s->loggrep(qr/^<<< Client$/) or die "no server input"
		    unless $args{server}{noin};
	}
}

1;
