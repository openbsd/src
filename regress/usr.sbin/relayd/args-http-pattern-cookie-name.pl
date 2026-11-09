# test that "pattern" enables patterns(7) matching for cookie name

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "mycookie=bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "^my%a+$" tag COOKIENOMATCH',
	    'match request cookie pattern "^MY" tag COOKIECASE',
	    'match request cookie pattern "^my%a+$" tag COOKIEPATTERN',
	],
	loggrep => {
	    qr/, COOKIENOMATCH,/ => 0,
	    qr/, COOKIECASE,/ => 0,
	    qr/, COOKIEPATTERN,.*done/ => 1,
	},
    },
    server => {
	func => \&http_server,
    },
);

1;
