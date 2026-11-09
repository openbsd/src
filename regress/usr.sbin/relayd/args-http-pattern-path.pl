# test that "pattern" enables patterns(7) matching for path

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "api/42",
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/api/%d+" tag PATHNOMATCH',
	    'match request path pattern "/api/%d+" tag PATHPATTERN',
	],
	loggrep => {
	    qr/, PATHNOMATCH,/ => 0,
	    qr/, PATHPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 42,
);

1;
