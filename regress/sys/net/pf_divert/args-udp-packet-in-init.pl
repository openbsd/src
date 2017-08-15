# test divert-packet input rule with udp initial packet

use strict;
use warnings;

our %args = (
    protocol => "udp",
    client => {
	func => \&write_read_datagram,
    },
    packet => {
	divertresp => 1,  # XXX the directions are broken
	func => \&read_write_packet,
	in => "Client",
    },
    server => {
	func => \&read_write_datagram,
	in => "Packet",
    },
    divert => "packet-in-init",
);
