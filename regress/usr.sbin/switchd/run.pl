#!/usr/bin/perl
# $OpenBSD: run.pl,v 1.12 2018/12/03 22:41:00 bluhm Exp $

# Copyright (c) 2017 Alexander Bluhm <bluhm@openbsd.org>
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
use File::Basename;
use IO::Socket::INET;
use Net::Pcap;
use NetPacket::Ethernet;
use NetPacket::IP;
use NetPacket::UDP;
use BSD::arc4random qw(arc4random arc4random_uniform arc4random_bytes);

use Switchd;

BEGIN {
	require OFP;
	require 'ofp.ph';
	require 'ofp10.ph';
}

sub fatal {
	my $class = shift;
	my $err = shift;
	print STDERR "*** ".$class.": ".$err."\n";
	die($err);
}

sub ofp_debug {
	my $dir = shift;
	my $ofp = shift;

	fatal("OFP", "empty response") if (!$ofp->{version});

	printf("OFP ".$dir." version %d type %d length %d xid %d\n",
	    $ofp->{version},
	    $ofp->{type},
	    $ofp->{length},
	    $ofp->{xid});

}

sub ofp_input {
	my $self = shift;
	my $pkt;
	my $pktext;
	my $ofp;
	my $ofplen;

	# Read the OFP payload head
	$self->{sock}->recv($pkt, 8);
	$ofp = NetPacket::OFP->decode($pkt) or
	    fatal('ofp_input', 'Failed to decode OFP header');

	# Read the body and decode it.
	$ofplen = $ofp->{length};
	if (defined($ofplen) && $ofplen > 8) {
		$ofplen -= 8;

		# Perl recv() only reads 16k at a time, so loop here.
		while ($ofplen > 0) {
			$self->{sock}->recv($pktext, $ofplen);
			if (length($pktext) == 0) {
				fatal('ofp_input', 'Socket closed');
			}
			$ofplen -= length($pktext);
			$pkt .= $pktext;
		}

		$ofp = NetPacket::OFP->decode($pkt) or
		    fatal('ofp_input', 'Failed to decode OFP');
	}
	ofp_debug('<', $ofp);

	return ($ofp);
}

sub ofp_output {
	my $self = shift;
	my $pkt = shift;
	my $ofp = NetPacket::OFP->decode($pkt);

	ofp_debug('>', $ofp);
	$self->{sock}->send($pkt);
}

sub ofp_match_align {
	my $matchlen = shift;

	return (($matchlen + (8 - 1)) & (~(8 - 1)));
}

sub ofp_hello {
	my $class;
	my $self = shift;
	my $hello = NetPacket::OFP->decode() or fatal($class, "new packet");
	my $features;
	my $pkt;

	$hello->{version} = $self->{version};
	$hello->{type} = OFP_T_HELLO();
	$hello->{xid} = $self->{xid}++;
	$pkt = NetPacket::OFP->encode($hello);

	# XXX timeout
	ofp_output($self, $pkt);
	$hello = ofp_input($self);

	# OpenFlow >= 1.3 wants features, set-config and table features.
	if ($self->{version} == OFP_V_1_3()) {
		$features = ofp_input($self);
		if ($features->{type} != OFP_T_FEATURES_REQUEST()) {
			fatal($class, 'Unexpected packet type ' .
			    $features->{type});
		}

		$pkt = NetPacket::OFP->decode() or
		    fatal($class, 'new packet');
		$pkt->{version} = $self->{version};
		$pkt->{type} = OFP_T_FEATURES_REPLY();
		$pkt->{xid} = $features->{xid};
		$pkt->{data} = pack('NNNCCxxNN',
		    0x00FFAABB, 0xCCDDEEFF,	# datapath_id
		    0,				# nbuffers
		    1,				# ntables
		    0,				# aux_id
		    0x00000001,			# capabilities
		    0x00000001			# actions
		    );
		ofp_output($self, NetPacket::OFP->encode($pkt));

		# Just read set-config and table features request
		ofp_input($self);
		ofp_input($self);

		# Answer the table features so switchd(8) install table-miss
		NetPacket::OFP->ofp_table_features_reply($self);
	}

	return ($hello);
}

sub ofp_packet_in {
	my $class;
	my $self = shift;
	my $data = shift;
	my $pktin = NetPacket::OFP->decode() or fatal($class, "new packet");
	my $pkt;

	if ($self->{version} == OFP_V_1_0()) {
		$pkt = pack('NnnCxa*',
		    OFP_PKTOUT_NO_BUFFER(),			# buffer_id
		    length($data),				# total_len
		    $self->{port} || OFP_PORT_NORMAL(),		# port
		    OFP_PKTIN_REASON_NO_MATCH(),		# reason
		    $data					# data
		    );
	} else {
		my $match = pack('nCCN',
		    OFP_OXM_C_OPENFLOW_BASIC(),			# class
		    OFP_XM_T_IN_PORT(),				# field + mask
		    4,						# length
		    $self->{port} || OFP_PORT_NORMAL()		# in_port
		    );
		# matchlen is OXMs + ofp_match header.
		my $matchlen = 4 + length($match);
		my $padding = ofp_match_align($matchlen) - $matchlen;
		if ($padding > 0) {
			$match .= pack("x[$padding]");
		}

		$pkt = pack('NnCCNNnna*xxa*',
		    OFP_PKTOUT_NO_BUFFER(),			# buffer_id
		    length($data),				# total_len
		    OFP_PKTIN_REASON_NO_MATCH(),		# reason
		    0,						# table_id
		    0x00000000, 0x00000000,			# cookie
		    OFP_MATCH_OXM(),				# match_type
		    $matchlen,					# match_len
		    $match,					# OXM matches
		    $data					# data
		    );
	}

	$pktin->{version} = $self->{version};
	$pktin->{type} = OFP_T_PACKET_IN();
	$pktin->{xid} = $self->{xid}++;
	$pktin->{data} = $pkt;
	$pkt = NetPacket::OFP->encode($pktin);

	# XXX timeout
	ofp_output($self, $pkt);
	return (ofp_input($self));
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
	$data = arc4random_bytes($packet->{length});

	# IP header
	$ip = NetPacket::IP->decode();
	$ip->{src_ip} = $packet->{src_ip} || "127.0.0.1";
	$ip->{dest_ip} = $packet->{dest_ip} || "127.0.0.1";
	$ip->{ver} = NetPacket::IP::IP_VERSION_IPv4;
	$ip->{hlen} = 5;
	$ip->{tos} = 0;
	$ip->{id} = arc4random_uniform(2**16);
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
	local $@;

	print "- $filename\n";

	require $path or fatal("main", $path);

	eval {
		$state = $func->init($sock);
	};
	die if($@);

	return if not $state->{pcap};

	$pcap_t = Net::Pcap::open_offline($dirs."".$state->{pcap}, \$err)
	    or fatal("main", $err);

	while ($pkt = Net::Pcap::next($pcap_t, \%hdr)) {

		$state->{data} = $pkt;
		$state->{eh} = packet_decode($pkt, \%hdr);

		eval {
			$func->next($state);
		};
		die if($@);
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

my $sd = Switchd->new(
    listenaddr          => "127.0.0.1",
    listenport          => 6633,
    testfile            => $test,
);
$sd->run->up;

# Open connection to the controller
my $sock = IO::Socket::INET->new(
	PeerHost => "127.0.0.1",
	PeerPort => 6633,
	Proto => 'tcp',
) or fatal("main", "ERROR in Socket Creation : $!\n");

# Run all requested tests
for my $test_file (@test_files) {
	process($sock, $test_file);
}

$sock->close();

$sd->kill_child->down;

1;
