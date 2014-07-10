# test persistent http 1.1 connection over http relay

use strict;
use warnings;

my @lengths = (251, 16384, 0, 1, 2, 3, 4, 5);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
    },
    relayd => {
	protocol => [ "http",
	    "match request header log foo",
	    "match response header log bar",
	],
	loggrep => qr/\, done/,
    },
    server => {
	func => \&http_server,
    },
    lengths => \@lengths,
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
