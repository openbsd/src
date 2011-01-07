# test emtpy server write when reverse sending

use strict;
use warnings;

our %args = (
    client => {
	func => \&read_char,
    },
    relay => {
	func => sub { ioflip(@_); relay(@_); },
    },
    server => {
	func => \&write_char,
	len => 0,
    },
    len => 0,
    md5 => "d41d8cd98f00b204e9800998ecf8427e",
);

1;
