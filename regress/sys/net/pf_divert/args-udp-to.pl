# test divert-to with udp

use strict;
use warnings;

our %args = (
    protocol => "udp",
    client => { func => \&write_datagram, noin => 1, },
    server => { func => \&read_datagram, noout => 1, },
    divert => "to",
);
