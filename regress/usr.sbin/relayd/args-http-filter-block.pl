# test http connection with request block filter, tests lateconnect

use strict;
use warnings;

my @lengths = (1, 2, 0, 3);
our %args = (
    client => {
	func => sub { eval { http_client(@_) }; warn $@ },
	loggrep => qr/Client missing http 3 response/,
	lengths => \@lengths,
    },
    relayd => {
	protocol => [ "http",
	    'request path filter "/3"',
	],
	loggrep => qr/rejecting request/,
    },
    server => {
	func => \&http_server,
	lengths => (1, 2, 0),
    },
);

1;
