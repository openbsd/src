# test divert-to with icmp

use strict;
use warnings;
use Socket;

our %args = (
	socktype => Socket::SOCK_RAW,
	protocol => sub { shift->{af} eq "inet" ? "icmp" : "icmp6" },
	client => { func => \&write_icmp_echo, out => "ICMP", noin => 1, },
	server => { func => \&read_icmp_echo, in => "ICMP", noout => 1, },
	divert => "to",
);
