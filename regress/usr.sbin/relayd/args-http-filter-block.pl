# test http block

use strict;
use warnings;

my @lengths = (1, 2, 0, 3);
our %args = (
    client => {
	func => sub { eval { http_client(@_) }; warn $@ },
	loggrep => qr/Client missing http 3 response/,
        lengths => \@lengths,
	mreqs => 1,
    },
    relayd => {
	protocol => [ "http",
	    'block request path "/3"',
	],
	loggrep => qr/Forbidden/,
    },
    server => {
	func => \&http_server,
	lengths => (1, 2, 0),
	mreqs => 3,
    },
);

1;
