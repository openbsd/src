# test that "value pattern" on a path rule matches the query string with
# case-sensitive patterns(7)

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "foo?A=B",
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/foo" value pattern "^a=%a$" tag PATHVALLOWER',
	    'match request path "/foo" value pattern "^A=%a$" tag PATHVALUPPER',
	],
	loggrep => {
	    qr/, PATHVALLOWER,/ => 0,
	    qr/, PATHVALUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 8,
);

1;
