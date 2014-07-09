# test chunked http 1.1 connection over http relay

use strict;
use warnings;

my @lengths = ([ 251, 10000, 10 ], 1, [2, 3]);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	mreqs => 1,
    },
    relayd => {
	protocol => [ "http",
	    "match request header log foo",
	    "match response header log Transfer-Encoding",
	],
	loggrep => {
		"Transfer-Encoding: chunked" => 2,
		qr/\[\(null\)\]/ => 0,
	},
    },
    server => {
	func => \&http_server,
	mreqs => scalar(@lengths),
    },
    lengths => \@lengths,
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
