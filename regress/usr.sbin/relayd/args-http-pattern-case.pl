# test that patterns(7) matching is always case-sensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "API/foo/bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request path pattern "^/api" tag PATLOWER',
	    'match request path pattern "^/API" tag PATUPPER',
	],
	loggrep => {
	    qr/, PATLOWER,/ => 0,
	    qr/, PATUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 12,
);

1;
