# test persistent http 1.1 connection and grep for content length

use strict;
use warnings;

my @lengths = (1, 2, 0, 3, 4);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	mreqs => 1,
    },
    relayd => {
	protocol => [ "http",
	    "match request header log foo",
	    "match response header log Content-Length",
	],
	loggrep => [ map { "Content-Length: $_" } @lengths ],
    },
    server => {
	func => \&http_server,
	mreqs => scalar(@lengths),
    },
);

1;
