use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	loggrep => qr/403 Forbidden/,
	path => "query?foo=bar&ok=yes",
	httpnok => 1,
    },
    relayd => {
	protocol => [ "http",
	    'return error',
	    'label "expect_foobar_label"',
	    'request query filter "bar" from "foo" log',
	    'no label',
	],
	loggrep => qr/.*403 Forbidden.*expect_foobar_label.*foo: bar/,
    },
    server => {
	noserver => 1,
	nocheck => 1,
    },
);

1;
