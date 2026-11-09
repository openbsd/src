# test that "pattern" enables patterns(7) matching for cookie value

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "session=abc123",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "session" value "%a+%d+" tag COOKIENOMATCH',
	    'match request cookie "session" ' .
		'value pattern "%a+%d+" tag COOKIEPATTERN',
	],
	loggrep => {
	    qr/, COOKIENOMATCH,/ => 0,
	    qr/, COOKIEPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
