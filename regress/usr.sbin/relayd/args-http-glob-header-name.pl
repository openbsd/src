# test that default glob(7) header name matching is case-insensitive

use strict;
use warnings;

my %header_client = (
	"X-Mixed-Case" => "ok",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header "x-MIXED-case" value "ok" tag HDRNAME',
	],
	loggrep => { qr/, HDRNAME,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
);

1;
