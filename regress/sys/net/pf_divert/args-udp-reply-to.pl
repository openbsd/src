# test divert-reply with udp with out and in packet

use strict;
use warnings;
use Socket;

our %args = (
    protocol => "udp",
    client => { func => \&write_read_datagram },
    server => { func => \&read_write_datagram },
    divert => "reply",
);
