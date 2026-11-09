# test that "glob ignorecase" matches header names case-insensitive,
# while patterns(7) stay case-sensitive

use strict;
use warnings;

my %header_client = (
	"X-Test" => "ok",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header pattern "^x%-t" tag HDRNOMATCH',
	    'match request header glob ignorecase "x-T*" value "ok" ' .
		'tag HDRICASE',
	],
	loggrep => {
	    qr/, HDRNOMATCH,/ => 0,
	    qr/, HDRICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
