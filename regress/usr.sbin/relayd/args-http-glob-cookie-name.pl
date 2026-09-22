# test that default glob(7) cookie name/key matching is case-insensitive

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	cookie => "myCOOKIE=bar",
    },
    relayd => {
	protocol => [ "http",
	    'match request cookie "MYcookie" value "bar" tag COOKIENAME',
	],
	loggrep => { qr/, COOKIENAME,.*done/ => 1 },
    },
    server => {
	func => \&http_server,
    },
);

1;
