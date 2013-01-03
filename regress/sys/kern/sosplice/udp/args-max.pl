# test maximum data length

use strict;
use warnings;

our %args = (
    client => {
	lengths => [ 1, 3, 1 ],
	func => sub { errignore(@_); write_datagram(@_); },
	nocheck => 1,
    },
    relay => {
	max => 4,
    },
    server => {
	num => 2,
    },
    len => 4,
    lengths => "1 3",
    md5 => "07515c3ca1f21779d63135eb5531eca2",
);
