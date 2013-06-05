# test persistent https connection with request filter

use strict;
use warnings;

my @lengths = (251, 16384, 0, 1, 2, 3, 4, 5);
our %args = (
    client => {
	# relayd closes the connection on the first blocked request
	func => sub { eval { http_client(@_) }; warn $@ },
	lengths => \@lengths,
	loggrep => qr/Client missing http 2 response/,
	ssl => 1,
    },
    relayd => {
	protocol => [ "http",
	    'request path filter "/2"',
	],
	loggrep => [
	    qr/ssl, ssl client/,
	    qr/rejecting request/,
	],
	forwardssl => 1,
	listenssl => 1,
    },
    server => {
	func => \&http_server,
	ssl => 1,
    },
    lengths => [251, 16384, 0, 1],
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
