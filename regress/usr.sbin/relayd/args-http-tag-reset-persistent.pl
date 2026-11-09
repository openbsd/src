# test that a tag set at the start of each request resets the tag of a
# persistent connection, the request for /3 is not blocked anymore

use strict;
use warnings;

my @lengths = (251, 16384, 0, 1, 2, 3);
our %args = (
    client => {
	func => \&http_client,
	lengths => \@lengths,
	http_vers => ["1.1"],
    },
    relayd => {
	protocol => [ "http",
	    'match request tag "RESET"',
	    'match request path "/1" tag "AB"',
	    'block request quick tagged "AB" path "/3"',
	],
	loggrep => {
	    qr/Forbidden/ => 0,
	},
    },
    server => {
	func => \&http_server,
    },
    lengths => \@lengths,
    md5 => [
	"bc3a3f39af35fe5b1687903da2b00c7f",
	"52afece07e61264c3087ddf52f729376",
	"d41d8cd98f00b204e9800998ecf8427e",
	"68b329da9893e34099c7d8ad5cb9c940",
	"897316929176464ebc9ad085f31e7284",
	"0ade138937c4b9cb36a28e2edb6485fc",
    ],
);

1;
