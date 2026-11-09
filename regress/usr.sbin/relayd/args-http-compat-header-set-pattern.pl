# test that "header set pattern" changes the value of a header matching
# the patterns(7) key, "pattern" was accepted with set before

use strict;
use warnings;

my %header_client = (
	"X-Test" => "old",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header set pattern "^X%-Test$" value "new"',
	],
    },
    server => {
	func => \&http_server,
	loggrep => {
	    "X-Test: old" => 0,
	    "X-Test: new" => 1,
	},
    },
);

1;
