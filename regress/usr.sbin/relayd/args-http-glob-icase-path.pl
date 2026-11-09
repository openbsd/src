# test that "glob ignorecase" makes path matching case-insensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "FOO",
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/foo" tag PATHNOMATCH',
	    'match request path glob ignorecase "/foo" tag PATHICASE',
	],
	loggrep => {
	    qr/, PATHNOMATCH,/ => 0,
	    qr/, PATHICASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 4,
);

1;
