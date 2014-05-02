use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	path => "a/b/c/d/e/f/gindex.html",
	loggrep => [
	    qr/403 Forbidden/,
	    qr/Server: OpenBSD relayd/,
	    qr/Connection: close/,
	],
	httpnok => 1,
    },
    relayd => {
	protocol => [ "http",
	    'return error',
	    'request url filter "foo.bar/a/b/" log',
	],
	loggrep => {
	    qr/rejecting request \(403 Forbidden\)/ => 1,
	    qr/\[foo.bar\/a\/b\/:/ => 1,
	},
    },
    server => {
	noserver => 1,
	nocheck => 1,
    },
);

1;
