# test inline out-of-band data when reverse sending with non-blocking relay

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
	nonblocking => 1,
    },
    server => {
	func => \&write_oob,
    },
    len => 251,
    md5 => [
	"24b69642243fee9834bceee5b47078ae",
	"5aa8135a1340e173a7d7a5fa048a999e",
    ],
);

1;
