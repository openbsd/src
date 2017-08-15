# test divert-packet output rule with udp initial packet

use strict;
use warnings;

our %args = (
    protocol => "udp",
    client => {
	func => \&write_read_datagram,
    },
    packet => {
	divertinit => 1,  # XXX the directions are completely broken
	func => \&read_write_packet,
	in => "Client",
    },
    server => {
	func => \&read_write_datagram,
	in => "Packet",
    },
    divert => "packet-out-init",
);
