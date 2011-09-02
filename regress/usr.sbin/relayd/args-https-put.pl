# test persistent https 1.1 put over http relay

use strict;
use warnings;

my @lengths = (251, 16384, 0, 1, 2, 3, 4, 5);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	method => "PUT",
	ssl => 1,
    },
    relay => {
	protocol => [ "http",
	    "request header log foo",
	    "response header log bar",
	],
	forwardssl => 1,
	listenssl => 1,
    },
    server => {
	func => \&http_server,
	ssl => 1,
    },
    lengths => \@lengths,
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
