# test inline out-of-band data with non-blocking relay

use strict;
use warnings;

our %args = (
    client => {
	func => \&write_oob,
    },
    relay => {
	oobinline => 1,
	nonblocking => 1,
    },
    server => {
	func => \&read_oob,
	oobinline => 1,
    },
    len => 251,
    md5 => "24b69642243fee9834bceee5b47078ae",
);

1;
