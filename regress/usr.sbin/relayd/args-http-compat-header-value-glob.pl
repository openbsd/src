# test that a header value with glob(7) characters and no matching keyword
# is a case-sensitive glob(7), as before the glob keywords

use strict;
use warnings;

my %header_client = (
	"X-Test" => "HELLO",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header "X-Test" value "hel*" tag HDRLOWER',
	    'match request header "X-Test" value "H?L*" tag HDRUPPER',
	],
	loggrep => {
	    qr/, HDRLOWER,/ => 0,
	    qr/, HDRUPPER,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
