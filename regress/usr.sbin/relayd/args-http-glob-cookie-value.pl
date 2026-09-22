# test that default glob(7) cookie value matching is case-insensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=BAR",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "mycookie" value "bar" tag COOKIEVAL',
	],
	loggrep => { qr/, COOKIEVAL,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
);

1;
