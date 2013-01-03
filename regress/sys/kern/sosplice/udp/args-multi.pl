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
	num => scalar @lengths,
	rcvbuf => 20000,
    },
    len => sum(@lengths),
    lengths => "@lengths",
    md5 => "f5b58b46c97b566fc8d34080c475d637",
);
