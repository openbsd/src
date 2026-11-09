# test that "pattern" enables patterns(7) matching for header value

use strict;
use warnings;

my %header_client = (
	"X-Version" => "v1.2.3",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header "X-Version" ' .
		'value "v%d+%.%d+%.%d+" tag HDRNOMATCH',
	    'match request header "X-Version" ' .
		'value pattern "v%d+%.%d+%.%d+" tag HDRPATTERN',
	],
	loggrep => {
	    qr/, HDRNOMATCH,/ => 0,
	    qr/, HDRPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
