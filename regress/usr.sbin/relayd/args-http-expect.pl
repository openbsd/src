use strict;
use warnings;

my @lengths = (21);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	path => "query?foo=bar&ok=yes"
    },
    relayd => {
	protocol => [ "http",
	    'request query expect "bar" from "foo" log',
	],
    },
    server => {
	func => \&http_server,
    },
    lengths => \@lengths,
);

1;
