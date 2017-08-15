# test divert-packet input rule with udp response packet

use strict;
use warnings;

our %args = (
    protocol => "udp",
    client => {
	func => \&write_read_datagram,
	in => "Packet",
    },
    packet => {
	divertinit => 1,  # XXX the directions are broken
	func => \&read_write_packet,
	in => "Server",
    },
    server => {
	func => \&read_write_datagram,
    },
    divert => "packet-in-resp",
);
