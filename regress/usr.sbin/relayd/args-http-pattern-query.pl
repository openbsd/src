# test that "pattern" enables patterns(7) matching for query value

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "search?id=42",
    },
    relayd => {
	protocol => [ "http",
	    'match request query "id" value "%d+" tag QUERYNOMATCH',
	    'match request query "id" value pattern "%d+" tag QUERYPATTERN',
	],
	loggrep => {
	    qr/, QUERYNOMATCH,/ => 0,
	    qr/, QUERYPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 42,
);

1;
