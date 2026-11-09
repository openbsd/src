# test that a header name with glob(7) characters and no matching keyword
# is a case-insensitive glob(7), as before the glob keywords

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
	    'match request header "x-nomatch*" tag HDRNOMATCH',
	    'match request header "x-t?S*" value "ok" tag HDRGLOB',
	],
	loggrep => {
	    qr/, HDRNOMATCH,/ => 0,
	    qr/, HDRGLOB,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
