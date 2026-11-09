# test that "header remove pattern" removes a header matching the
# patterns(7) key

use strict;
use warnings;

my %header_client = (
	"X-Test" => "remove",
	"X-Other" => "keep",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header remove pattern "^X%-T"',
	],
    },
    server => {
	func => \&http_server,
	loggrep => {
	    "X-Test: remove" => 0,
	    "X-Other: keep" => 1,
	},
    },
);

1;
