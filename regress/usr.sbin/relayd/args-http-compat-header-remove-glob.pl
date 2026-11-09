# test that "header remove" with glob(7) characters and no matching keyword
# removes a header matching case-insensitive, as before the glob keywords

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
	    'match request header remove "x-t*"',
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
