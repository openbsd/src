# test divert-reply with udp with out and in packet

use strict;
use warnings;
use Socket;

our %args = (
    protocol => "udp",
    client => {
	func => sub {
	    my $self = shift;
	    write_datagram($self);
	    read_datagram($self);
	},
    },
    server => {
	func => sub {
	    my $self = shift;
	    read_datagram($self);
	    $self->{toaddr} = $self->{fromaddr};
	    $self->{toport} = $self->{fromport};
	    write_datagram($self);
	},
    },
    divert => "reply",
);
