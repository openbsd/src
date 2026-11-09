# test that "pattern" enables patterns(7) matching for query name

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "search?id=42",
    },
    relayd => {
	protocol => [ "http",
	    'match request query "^i%a$" tag QUERYNOMATCH',
	    'match request query pattern "^I" tag QUERYCASE',
	    'match request query pattern "^i%a$" tag QUERYPATTERN',
	],
	loggrep => {
	    qr/, QUERYNOMATCH,/ => 0,
	    qr/, QUERYCASE,/ => 0,
	    qr/, QUERYPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 42,
);

1;
