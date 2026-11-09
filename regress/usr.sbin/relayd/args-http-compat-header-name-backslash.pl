# test that a header name with a backslash but no other glob(7) character
# is compared literally, as before the glob keywords

use strict;
use warnings;

my %header_client = (
	"X-Test" => "ok",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header "X\-Test" tag HDRESCAPE',
	    'match request header "x-test" tag HDRLITERAL',
	],
	loggrep => {
	    qr/, HDRESCAPE,/ => 0,
	    qr/, HDRLITERAL,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
