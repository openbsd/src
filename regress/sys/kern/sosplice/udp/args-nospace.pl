# test sending an udp packet that does not fit into splice space

use strict;
use warnings;

our %args = (
    client => {
	lengths => [ 1, 10000, 2, 10001, 3 ],
	sndbuf => 30000,
	nocheck => 1,
    },
    relay => {
	rcvbuf => 30000,
	sndbuf => 10000,
    },
    server => {
	num => 4,
	rcvbuf => 30000,
    },
    len => 10006,
    lengths => "1 10000 2 3",
    md5 => "90c347b1853f03d6e73aa88c9d12ce55",
);
