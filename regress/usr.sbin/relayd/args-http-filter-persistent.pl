# test persistent http connection with request filter

use strict;
use warnings;

my @lengths = (251, 16384, 0, 1, 2, 3, 4, 5);
our %args = (
    client => {
	# relayd closes the connection on the first blocked request
	func => sub { eval { http_client(@_) }; warn $@ },
	lengths => \@lengths,
	loggrep => qr/Client missing http 2 response/,
	mreqs => 1,
	httpnok => 1,
    },
    relayd => {
	protocol => [ "http",
	    'block request path "/2"',
	],
	loggrep => qr/Forbidden/,
    },
    server => {
	func => \&http_server,
	mreqs => 7,
	nocheck => 1,
    },
    lengths => [251, 16384, 0, 1],
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
