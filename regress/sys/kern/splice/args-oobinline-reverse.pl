# test inline out-of-band data when reverse sending

use strict;
use warnings;

our %args = (
    client => {
	func => \&read_oob,
	oobinline => 1,
    },
    relay => {
	func => sub { ioflip(@_); relay(@_); },
	oobinline => 1,
    },
    server => {
	func => \&write_oob,
    },
    len => 251,
    md5 => "24b69642243fee9834bceee5b47078ae",
);

1;
