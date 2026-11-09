# test that "header set" with "value pattern" sets the value literally
# without macro expansion, "pattern" was accepted with set before

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
	    'match request header set "X-Test" value pattern "$REMOTE_ADDR"',
	],
    },
    server => {
	func => \&http_server,
	loggrep => {
	    qr/X-Test: \$REMOTE_ADDR/ => 1,
	    qr/X-Test: old/ => 0,
	},
    },
);

1;
