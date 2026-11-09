# test that a tag stays on a persistent connection, the request for /3
# is blocked by the tag set for /1 before

use strict;
use warnings;

my @lengths = (251, 16384, 0, 1, 2, 3);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	http_vers => ["1.1"],
	loggrep => qr/Client missing http 3 response/,
    },
    relayd => {
	protocol => [ "http",
	    'match request path "/1" tag "AB"',
	    'block request quick tagged "AB" path "/3"',
	],
	loggrep => qr/Forbidden/,
    },
    server => {
	func => \&http_server,
    },
    lengths => [251, 16384, 0, 1, 2],
    md5 => [
	"bc3a3f39af35fe5b1687903da2b00c7f",
	"52afece07e61264c3087ddf52f729376",
	"d41d8cd98f00b204e9800998ecf8427e",
	"68b329da9893e34099c7d8ad5cb9c940",
	"897316929176464ebc9ad085f31e7284",
    ],
);

1;
