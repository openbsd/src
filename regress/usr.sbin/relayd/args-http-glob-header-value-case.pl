# test that default glob(7) header value matching is case-sensitive

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
	    'match request header "X-Test" value "hello" tag HDRLOWER',
	    'match request header "X-Test" value "HELLO" tag HDRUPPER',
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
