# test that "path log" and "query log" without a key match anything, the
# implicit "*" key is a glob(7), as before the glob keywords

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "search?id=42",
    },
    relayd => {
	protocol => [ "http",
	    'match request path log tag PATHLOG',
	    'match request query log tagged PATHLOG tag QUERYLOG',
	],
	loggrep => { qr/, QUERYLOG,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
    len => 42,
);

1;
