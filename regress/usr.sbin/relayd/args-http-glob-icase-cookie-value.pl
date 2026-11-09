# test that "glob ignorecase" syntax is accepted for cookie value matching

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=BAR",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "mycookie" ' .
		'value glob ignorecase "bar" tag COOKIEICASE',
	],
	loggrep => { qr/, COOKIEICASE,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
);

1;
