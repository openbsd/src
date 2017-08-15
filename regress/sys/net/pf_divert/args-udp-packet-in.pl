# test divert-packet input rule with udp

use strict;
use warnings;

our %args = (
    protocol => "udp",
    client => {
	func => \&write_datagram,
	noin => 1,
    },
    packet => {
	func => \&read_write_packet,
	in => "Client",
    },
    server => {
	func => \&read_datagram,
	in => "Packet",
	noout => 1,
    },
    divert => "packet-in",
);
