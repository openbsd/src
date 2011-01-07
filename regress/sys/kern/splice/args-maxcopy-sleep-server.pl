# test relay maximum data length then copy with delay before server

use strict;
use warnings;

our %args = (
    relay => {
	func => sub { relay(@_, 61); relay_copy(@_); },
	nocheck => 1,
    },
    server => {
	func => sub { sleep 3; read_char(@_); },
    },
    len => 251,
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
