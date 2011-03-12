# test immediate data transfer after maximum data length

use strict;
use warnings;

our %args = (
    client => {
	func => sub { $| = 1; errignore(@_); write_char(@_); sleep(6); },
	nocheck => 1,
    },
    relay => {
	func => sub { relay(@_); sleep(5); },
	max => 63,
    },
    server => {
	func => sub { alarm(4); read_char(@_); },
	max => 63,
    },
    len => 63,
    md5 => "4a3c3783fc56943715c53e435973b2ee",
);

1;
