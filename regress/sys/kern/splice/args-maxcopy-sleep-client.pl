# test relay maximum data length then copy with delay before client

use strict;
use warnings;

our %args = (
    client => {
	func => sub { sleep 3; write_char(@_); },
    },
    relay => {
	func => sub { relay(@_, 61); relay_copy(@_); },
	nocheck => 1,
    },
    len => 251,
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
