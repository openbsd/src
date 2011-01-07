# test maximum data length with delay before client write with non-blocking relay

use strict;
use warnings;

our %args = (
    client => {
	func => sub { sleep 3; write_char(@_); },
	nocheck => 1,
    },
    relay => {
	max => 113,
	nonblocking => 1,
    },
    len => 113,
    md5 => "dc099ef642faa02bce71298f11e7d44d",
);

1;
