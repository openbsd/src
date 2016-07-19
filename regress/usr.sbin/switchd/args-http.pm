# $OpenBSD: args-http.pm,v 1.1 2016/07/19 17:19:58 reyk Exp $

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

package args_http;

use strict;
use warnings;
use base qw(Exporter);
our @EXPORT = qw(init next);

my $topology = {
	hosts => {
		"6c8814709208" => {
			"port" => 8
		},
		"3431c4778157" => {
			"port" => 24
		}
	}
};

sub init {
	my $class = shift;
	my $sock = shift;
	my $self = { "count" => 0, "pcap" => "args-http.pcap",
	    "sock" => $sock, "version" => main::OFP_V_1_0() };

	bless($self, $class);

	main::ofp_hello($self);

	return ($self);
}

sub next {
	my $class = shift;
	my $self = shift;
	my $src;

	$self->{count}++;

	$src = $topology->{hosts}->{$self->{eh}->{src_mac}};
	if ($src) {
		$self->{port} = $src->{port};
	}

	main::ofp_packet_in($self, $self->{data});
}

1;
