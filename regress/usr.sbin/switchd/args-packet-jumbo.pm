# $OpenBSD: args-packet-jumbo.pm,v 1.4 2016/11/17 14:37:55 rzalamena Exp $

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

package args_packet_jumbo;

use strict;
use warnings;
use base qw(Exporter);
our @EXPORT = qw(init next);

my $topology = {
	buffers => {},
	hosts => {
		"a00000000001" => {
			"port" => 1
		},
		"a00000000002" => {
			"port" => 2
		},
		"a00000000003" => {
			"port" => 3
		}
	},
	packets => [
		{
			"src_mac" => "a00000000001",
			"dest_mac" => "a00000000002",
			"src_ip" => "10.0.0.1",
			"src_port" => 12345,
			"dest_ip" => "10.0.0.2",
			"dest_port" => 80,
			"length" => 2048,
			"count" => 3,
			"ofp_response" => main::OFP_T_PACKET_OUT()
		},
		{
			"src_mac" => "a00000000002",
			"dest_mac" => "a00000000001",
			"src_ip" => "10.0.0.2",
			"src_port" => 80,
			"dest_ip" => "10.0.0.1",
			"dest_port" => 12345,
			"length" => 17000,
			"count" => 3,
			"ofp_response" => main::OFP_T_FLOW_MOD()

		},
		{
			"src_mac" => "a00000000001",
			"dest_mac" => "ffffffffffff",
			"src_ip" => "10.0.0.1",
			"dest_ip" => "10.255.255.255",
			"length" => 65451,
			"count" => 3,
			"ofp_response" => main::OFP_T_PACKET_OUT()
		}
	]
};

sub init {
	my $class = shift;
	my $sock = shift;
	my $self = { "count" => 0,
	    "sock" => $sock, "version" => main::OFP_V_1_3() };

	bless($self, $class);
	main::ofp_hello($self);

	for (my $i = 0; $i < @{$topology->{packets}}; $i++) {
		my $packet = $topology->{packets}[$i];
		my $src = $topology->{hosts}->{$packet->{src_mac}};

		$self->{port} = $src->{port} if ($src);

		for (my $j = 0; $j < $packet->{count}; $j++) {
			my $ofp;
			$self->{count}++;
			$ofp = main::packet_send($self, $packet);

			if (not defined($packet->{ofp_response})) {
				continue;
			}

			if ($ofp->{type} != $packet->{ofp_response}) {
				main::fatal($class,
				    "invalid ofp response type " .
				    $ofp->{type});
			}

			# Flow-mod also expects an packet-out.
			if ($packet->{ofp_response} == main::OFP_T_FLOW_MOD()) {
				$ofp = main::ofp_input($self);
				if ($ofp->{type} != main::OFP_T_PACKET_OUT()) {
					main::fatal($class,
					    "invalid ofp response type " .
					    $ofp->{type});
				}
			}
		}
	}

	return ($self);
}

sub next {
	# Not used
}

1;
