#!/usr/bin/perl -w
# $OpenBSD: run.pl,v 1.1 2016/07/19 17:04:19 reyk Exp $

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

use strict;
use warnings;

BEGIN {
	require OFP;
	require 'ofp.ph';
	require 'ofp10.ph';
	require IO::Socket::INET;
}

use File::Basename;
use Net::Pcap;
use NetPacket::Ethernet;
use NetPacket::IP;
use NetPacket::UDP;
use Data::Dumper;
use Crypt::Random;

sub fatal {
	my $class = shift;
	my $err = shift;
	print STDERR "*** ".$class.": ".$err."\n";
	die($err);
}

sub ofp_debug {
	my $dir = shift;
	my $ofp = shift;

	printf("OFP ".$dir." version %d type %d length %d xid %d\n",
	    $ofp->{version},
	    $ofp->{type},
	    $ofp->{length},
	    $ofp->{xid});

}

sub ofp_hello {
	my $class;
	my $self = shift;
	my $hello = NetPacket::OFP->decode() or fatal($class, "new packet");
	my $resp = ();
	my $pkt;
	my $resppkt;

	$hello->{version} = $self->{version};
	$hello->{type} = OFP_T_HELLO();
	$hello->{xid} = $self->{xid}++;

	$pkt = NetPacket::OFP->encode($hello);

	ofp_debug(">", $hello);

	# XXX timeout
	$self->{sock}->send($pkt);
	$self->{sock}->recv($resppkt, 1024);

	$resp = NetPacket::OFP->decode($resppkt) or
	    fatal($class, "recv'ed packet");

	ofp_debug("<", $resp);

	return ($resp);
}

sub ofp_packet_in {
	my $class;
	my $self = shift;
	my $data = shift;
	my $pktin = NetPacket::OFP->decode() or fatal($class, "new packet");
	my $resp = ();
	my $pkt;
	my $resppkt;

	$pktin->{version} = $self->{version};
	$pktin->{type} = OFP_T_PACKET_IN();
	$pktin->{xid} = $self->{xid}++;
	$pktin->{data} = $data;
	$pktin->{length} += length($data);
	$pktin->{buffer_id} = $self->{count} || 1;
	$pktin->{port} = $self->{port} || OFP_PORT_NORMAL();

	$pkt = NetPacket::OFP->encode($pktin);

	ofp_debug(">", $pktin);

	# XXX timeout
	$self->{sock}->send($pkt);
	$self->{sock}->recv($resppkt, 1024);

	$resp = NetPacket::OFP->decode($resppkt) or
	    fatal($class, "recv'ed packet");

	ofp_debug("<", $resp);

	return ($resp);
}

sub packet_send {
	my $class;
	my $self = shift;
	my $packet = shift;
	my $eth;
	my $ip;
	my $udp;
	my $data;
	my $pkt;
	my $src;

	# Payload
	$data = Crypt::Random::makerandom_octet(Length => $packet->{length});

	# IP header
	$ip = NetPacket::IP->decode();
	$ip->{src_ip} = $packet->{src_ip} || "127.0.0.1";
	$ip->{dest_ip} = $packet->{dest_ip} || "127.0.0.1";
	$ip->{ver} = NetPacket::IP::IP_VERSION_IPv4;
	$ip->{hlen} = 5;
	$ip->{tos} = 0;
	$ip->{id} = Crypt::Random::makerandom(Size => 16);
	$ip->{ttl} = 0x5a;
	$ip->{flags} = 0; #XXX NetPacket::IP::IP_FLAG_DONTFRAG;
	$ip->{foffset} = 0;
	$ip->{proto} = NetPacket::IP::IP_PROTO_UDP;
	$ip->{options} = '';

	# UDP header
	$udp = NetPacket::UDP->decode();
	$udp->{src_port} = $packet->{src_port} || 9000;
	$udp->{dest_port} = $packet->{dest_port} || 9000;
	$udp->{data} = $data;

	$ip->{data} = $udp->encode($ip);
	$pkt = $ip->encode() or fatal($class, "ip");

	# Create Ethernet header
	$self->{data} = pack('H12H12na*' ,
	    $packet->{dest_mac},
	    $packet->{src_mac},
	    NetPacket::Ethernet::ETH_TYPE_IP,
	    $pkt);

	return (main::ofp_packet_in($self, $self->{data}));
	
}

sub packet_decode {
	my $pkt = shift;
	my $hdr = shift;
	my $eh = NetPacket::Ethernet->decode($pkt);

	printf("%s %s %04x %d",
	    join(':', unpack '(A2)*', $eh->{src_mac}),
	    join(':', unpack '(A2)*', $eh->{dest_mac}),
	    $eh->{type}, length($pkt));
	if (length($pkt) < $hdr->{len}) {
		printf("/%d", $hdr->{len})
	}
	printf("\n");

	return ($eh);
}

sub process {
	my $sock = shift;
	my $path = shift;
	my $pcap_t;
	my $err;
	my $pkt;
	my %hdr;
	my ($filename, $dirs, $suffix) = fileparse($path, ".pm");
	(my $func = $filename) =~ s/-/_/g;
	my $state;

	print "- $filename\n";

	require $path or fatal("main", $path);

	eval {
		$state = $func->init($sock);
	};

	return if not $state->{pcap};

	$pcap_t = Net::Pcap::open_offline($dirs."".$state->{pcap}, \$err)
	    or fatal("main", $err);

	while ($pkt = Net::Pcap::next($pcap_t, \%hdr)) {

		$state->{data} = $pkt;
		$state->{eh} = packet_decode($pkt, \%hdr);

		eval {
			$func->next($state);
		};
	}

	Net::Pcap::close($pcap_t);
}

if (@ARGV < 1) {
    print "\nUsage: run.pl test.pl\n";
    exit;
}

# Flush after every write
$| = 1;

my $test = $ARGV[0];
my @test_files = ();
for (@ARGV) {
	push(@test_files, glob($_));
}

# Open connection to the controller
my $sock = new IO::Socket::INET(
	PeerHost => '127.0.0.1',
	PeerPort => '6633',
	Proto => 'tcp',
) or fatal("main", "ERROR in Socket Creation : $!\n");

# Run all requested tests
for my $test_file (@test_files) {
	process($sock, $test_file);
}

$sock->close();

1;
