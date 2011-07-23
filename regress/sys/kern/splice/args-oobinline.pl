# test inline out-of-band data

use strict;
use warnings;

our %args = (
    client => {
	func => \&write_oob,
    },
    relay => {
	oobinline => 1,
    },
    server => {
	func => \&read_oob,
	oobinline => 1,
    },
    len => 251,
    md5 => [
	"24b69642243fee9834bceee5b47078ae",
	"5aa8135a1340e173a7d7a5fa048a999e",
    ],
);

1;
