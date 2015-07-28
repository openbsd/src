# test divert-reply with icmp with out and in packet

use strict;
use warnings;
use Socket;

our %args = (
    socktype => Socket::SOCK_RAW,
    protocol => sub { shift->{af} eq "inet" ? "icmp" : "icmp6" },
    client => {
	func => sub {
	    my $self = shift;
	    write_icmp_echo($self);
	    read_icmp_echo($self, "reply");
	},
	out => "ICMP6?",
	in => "ICMP6? reply",
    },
    # no server as our kernel does the icmp reply automatically
    divert => "reply",
);
