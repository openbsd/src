# test http connection with request block filter, tests lateconnect

use strict;
use warnings;

our %args = (
    client => {
	func => sub {
	    eval { http_client(@_) };
	    warn $@;
	    $@ =~ /missing http 1 response/
		or die "http not filtered";
	},
	len => 1,
	nocheck => 1,
    },
    relayd => {
	protocol => [ "http",
	    'request path filter "/1"',
	],
	loggrep => qr/rejecting request/,
    },
    server => {
	noserver => 1,
	nocheck => 1,
    },
);

1;
