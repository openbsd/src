# $OpenBSD: args-icmp.pm,v 1.1 2016/07/19 17:19:58 reyk Exp $

# Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>
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

package args_icmp;

use strict;
use warnings;
use base qw(Exporter);
our @EXPORT = qw(init next);

sub init {
	my $class = shift;
	my $sock = shift;
	my $self = { "count" => 0, "pcap" => "args-icmp.pcap",
	    "sock" => $sock, "version" => main::OFP_V_1_0() };

	bless($self, $class);

	main::ofp_hello($self);

	return ($self);
}

sub next {
	my $class = shift;
	my $self = shift;

	$self->{count}++;
	$self->{port} = $self->{count} % 2;

	main::ofp_packet_in($self, $self->{data});
}

1;
