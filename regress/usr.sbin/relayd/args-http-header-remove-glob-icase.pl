# test that "header remove glob ignorecase" removes a header matching
# case-insensitive and keeps the others

use strict;
use warnings;

my %header_client = (
	"X-Debug-Trace" => "remove",
	"X-Other" => "keep",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header remove glob ignorecase "x-debug-*"',
	],
    },
    server => {
	func => \&http_server,
	loggrep => {
	    "X-Debug-Trace: remove" => 0,
	    "X-Other: keep" => 1,
	},
    },
);

1;
