# test that "glob ignorecase" makes header value matching case-insensitive

use strict;
use warnings;

my %header_client = (
	"X-Test" => "HELLO",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header "X-Test" value "hello" tag HDRNOMATCH',
	    'match request header "X-Test" ' .
		'value glob ignorecase "hello" tag HDRICASE',
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
