# test with mutiple packets

use strict;
use warnings;
use List::Util qw(sum);

my @lengths = (251, 16384, 0, 1, 2, 3, 4, 5);

our %args = (
    client => {
	lengths => \@lengths,
	sndbuf => 20000,
    },
    relay => {
	rcvbuf => 20000,
	sndbuf => 20000,
    },
    server => {
	rcvbuf => 20000,
    },
    len => sum(@lengths),
    lengths => "@lengths",
    md5 => "544464f20384567028998e1a1a4c5b1e",
);
