#	$OpenBSD: funcs.pl,v 1.4 2013/06/05 04:34:27 bluhm Exp $

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

sub in_cksum {
	my $data = shift;
	my $sum = 0;

	$data .= pack("x") if (length($data) & 1);
	while (length($data)) {
		$sum += unpack("n", substr($data, 0, 2, ""));
		$sum = ($sum >> 16) + ($sum & 0xffff) if ($sum > 0xffff);
	}
	return (~$sum & 0xffff);
}

use constant IPPROTO_ICMPV6	=> 58;
use constant ICMP_ECHO		=> 8;
use constant ICMP6_ECHO_REQUEST	=> 128;

my $seq = 0;
sub write_icmp_echo {
	my $self = shift;
	my $af = $self->{af};

	my $type = $af eq "inet" ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
	# type, code, cksum, id, seq
	my $icmp = pack("CCnnn", $type, 0, 0, $$, ++$seq);
	if ($af eq "inet") {
		substr($icmp, 2, 2, pack("n", in_cksum($icmp)));
	} else {
		# src, dst, plen, pad, next
		my $phdr = "";
		$phdr .= inet_pton(AF_INET6, $self->{srcaddr});
		$phdr .= inet_pton(AF_INET6, $self->{dstaddr});
		$phdr .= pack("NxxxC", length($icmp), IPPROTO_ICMPV6);
		print STDERR "pseudo header: ", unpack("H*", $phdr), "\n";
		substr($icmp, 2, 2, pack("n", in_cksum($phdr. $icmp)));
	}

	print $icmp;
	IO::Handle::flush(\*STDOUT);
	my $text = $af eq "inet" ? "ICMP" : "ICMP6";
	print STDERR ">>> $text ", unpack("H*", $icmp), "\n";
}

sub read_icmp_echo {
	my $self = shift;
	my $af = $self->{af};

	# Raw sockets include the IPv4 header.
	sysread(STDIN, my $icmp, 70000);
	# Cut the header off.
	if ($af eq "inet") {
		substr($icmp, 0, 20, "");
	}

	my $text = $af eq "inet" ? "ICMP" : "ICMP6";
	my $phdr = "";
	if ($af eq "inet6") {
		# src, dst, plen, pad, next
		$phdr .= inet_pton(AF_INET6, $self->{srcaddr});
		$phdr .= inet_pton(AF_INET6, $self->{dstaddr});
		$phdr .= pack("NxxxC", length($icmp), IPPROTO_ICMPV6);
		print STDERR "pseudo header: ", unpack("H*", $phdr), "\n";
	}
	if (length($icmp) < 8) {
		$text = "BAD $text LENGTH";
	} elsif (in_cksum($phdr. $icmp) != 0) {
		$text = "BAD $text CHECKSUM";
	} else {
		my($type, $code, $cksum, $id, $seq) = unpack("CCnnn", $icmp);
		if ($type != ($af eq "inet" ? ICMP_ECHO : ICMP6_ECHO_REQUEST)) {
			$text = "BAD $text TYPE";
		} elsif ($code != 0) {
			$text = "BAD $text CODE";
		}
	}

	print STDERR "<<< $text ", unpack("H*", $icmp), "\n";
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
		my $out = $args{client}{out} || "Client";
		$c->loggrep(qr/^>>> $out/) or die "no client output"
		    unless $args{client}{noout};
		my $in = $args{client}{in} || "Server";
		$c->loggrep(qr/^<<< $in/) or die "no client input"
		    unless $args{client}{noin};
	}
	if ($s && !$args{server}{nocheck}) {
		my $out = $args{server}{out} || "Server";
		$s->loggrep(qr/^>>> $out/) or die "no server output"
		    unless $args{server}{noout};
		my $in = $args{server}{in} || "Client";
		$s->loggrep(qr/^<<< $in/) or die "no server input"
		    unless $args{server}{noin};
	}
}

1;
