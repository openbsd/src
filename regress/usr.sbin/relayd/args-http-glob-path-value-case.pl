# test that the path rule value (query string) defaults to a
# case-insensitive glob(7) and "value glob" makes it case-sensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "foo?A=B",
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/foo" value "a=b" tag PATHVALDEFAULT',
	    'match request path "/foo" value glob "a=b" ' .
		'tagged PATHVALDEFAULT tag PATHVALGLOB',
	    'match request path "/foo" value glob "A=B" ' .
		'tagged PATHVALDEFAULT tag PATHVALCASE',
	],
	loggrep => {
	    qr/, PATHVALGLOB,/ => 0,
	    qr/, PATHVALCASE,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
    len => 8,
);

1;
