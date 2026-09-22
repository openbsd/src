# test that an omitted rule value defaults to '*' and matches any actual value,
# both with a plain glob(7) key and with a patterns(7) key

use strict;
use warnings;

my %header_client = (
	"X-Test" => "whatever",
);

our %args = (
    client => {
	func => \&http_client,
	header => \%header_client,
    },
    relayd => {
	protocol => [ "http",
	    'match request header "X-Test" tag VALGLOB',
	    'match request header pattern "^X%-Test$" tagged VALGLOB tag VALPAT',
	],
	loggrep => { qr/, VALPAT,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
);

1;
