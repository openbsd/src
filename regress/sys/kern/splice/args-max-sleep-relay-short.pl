# test maximum data length with delay before relay copy and short len

use strict;
use warnings;

our %args = (
    client => {
	nocheck => 1,
    },
    relay => {
	func => sub { sleep 3; relay(@_); },
	max => 113,
    },
    len => 113,
    md5 => "dc099ef642faa02bce71298f11e7d44d",
);

1;
