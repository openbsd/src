# test that "glob" on header names is turned into "glob ignorecase"
# with a warning, header names are case-insensitive (RFC 9110)

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
	    'match request header glob "x-t*" tag HDRGLOB',
	],
	loggrep => {
	    qr/header x-t\*: names are case-insensitive/ => 1,
	    qr/, HDRGLOB,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
