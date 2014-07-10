# test persistent https connection with request filter

use strict;
use warnings;

my @lengths = (251, 16384, 0, 1, 2, 3, 4, 5);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	loggrep => qr/Client missing http 2 response/,
	ssl => 1,
    },
    relayd => {
	protocol => [ "http",
	    'block request path "/2"',
	],
	loggrep => [
	    qr/ssl, ssl client/ => 1,
	    qr/Forbidden/ => 1,
	],
	forwardssl => 1,
	listenssl => 1,
    },
    server => {
	func => \&http_server,
	ssl => 1,
    },
    lengths => [251, 16384, 0, 1, 3, 4, 5],
    md5 => "bc3a3f39af35fe5b1687903da2b00c7f",
);

1;
