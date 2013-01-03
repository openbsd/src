# test idle timeout

use strict;
use warnings;
use List::Util qw(sum);

my @lengths = (0, 1, 2, 3, 4, 5);

our %args = (
    client => {
	lengths => \@lengths,
	sleep => 1,
    },
    relay => {
	idle => 2,
	timeout => 1,
    },
    server => {
	num => scalar @lengths,
    },
    len => sum(@lengths),
    lengths => "@lengths",
    md5 => "a5612eb5137e859bf52c515a890241a5",
);
