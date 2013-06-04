# test divert-reply with raw ip

use strict;
use warnings;
use Socket;

our %args = (
	socktype => Socket::SOCK_RAW,
	protocol => 254,
	skip => sub { shift->{af} eq "inet" ? 20 : 0 },
	client => { func => \&write_datagram, noin => 1, },
	server => { func => \&read_datagram, noout => 1, },
	divert => "reply",
);
